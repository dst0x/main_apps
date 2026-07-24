##############################################################################
# Makefile -- STM32F412RETx, build + flash, pick which App/app_<name>/ to build
#
#   make                 build App/app_bgt/          (APP defaults to "bgt")
#   make APP=bgt          "
#   make APP=xyz          build App/app_xyz/ instead (folder must exist)
#   make list-apps        show every App/app_* folder available to build
#   make flash            build, then flash over ST-Link
#   make clean            remove build/ for the CURRENT APP only
#   make clean-all        remove build/ entirely (every app's output)
#
# Requires:
#   - arm-none-eabi-gcc/objcopy/size in PATH (GNU Arm Embedded Toolchain)
#   - a POSIX-ish shell for `make` itself (Git Bash / MSYS2 / WSL on Windows)
#   - STM32_Programmer_CLI in PATH for `make flash` (STM32CubeProgrammer)
##############################################################################

APP     ?= esp32
APP_DIR := App/app_$(APP)

ifeq ($(wildcard $(APP_DIR)),)
$(error Unknown APP "$(APP)" -- no such folder $(APP_DIR). Run "make list-apps" to see what's available)
endif

APP_UPPER  := $(shell echo $(APP) | tr '[:lower:]' '[:upper:]')
APP_DEFINE := -DAPP_$(APP_UPPER)

TARGET    := firmware_$(APP)
BUILD_DIR := build/$(APP)

##############################################################################
# Toolchain
##############################################################################
PREFIX  ?= arm-none-eabi-
CC      := $(PREFIX)gcc
AS      := $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

# Flash tool -- STM32CubeProgrammer CLI by default, using its full install
# path so `make flash` works even when the bin/ folder isn't on THIS shell's
# PATH (only make's own PATH had it last time). Override on the command line
# if yours lives elsewhere, e.g.:
#   make flash FLASH_TOOL="D:/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe"
# Or swap FLASH_TOOL/FLASH_CMD entirely for a different tool, e.g.:
#   st-flash : FLASH_CMD = st-flash --reset write $(BUILD_DIR)/$(TARGET).bin 0x08000000
#   OpenOCD  : FLASH_CMD = openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
#                            -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"
FLASH_TOOL ?= C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe
FLASH_CMD  := "$(FLASH_TOOL)" -c port=SWD -w $(BUILD_DIR)/$(TARGET).hex -v -rst

##############################################################################
# MCU / build flags
##############################################################################
DEBUG ?= 0

MCU_FLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

DEFS := -DSTM32F412Rx -DUSE_HAL_DRIVER $(APP_DEFINE)

INCLUDES := \
  -ICore/Inc \
  -IApp \
  -I$(APP_DIR) \
  -ISensors \
  -IDrivers/STM32F4xx_HAL_Driver/Inc \
  -IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy \
  -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
  -IDrivers/CMSIS/Include \
  -IMiddlewares/threadx/common/inc \
  -IMiddlewares/threadx/ports/cortex_m4/gnu/inc \
  -IMiddlewares/filex/common/inc \
  -IMiddlewares/filex/ports/cortex_m4/gnu/inc \
  -IMiddlewares/modbus \
  -IMiddlewares/log/inc \
  -IMiddlewares/esp_at

ifeq ($(DEBUG),1)
OPT := -Og -g3
else
OPT := -O2
endif

CFLAGS  := $(MCU_FLAGS) $(DEFS) $(INCLUDES) $(OPT) -Wall \
           -ffunction-sections -fdata-sections -std=gnu11 -MMD -MP
ASFLAGS := $(MCU_FLAGS) $(DEFS) $(INCLUDES)

LDSCRIPT := STM32F412RETX_FLASH.ld
# -Wl,-u,_printf_float: nano.specs' printf() drops %f support to save space by
# default; LOG_INFO() below prints sensor floats, so pull the float-capable
# printf back in.
LDFLAGS  := $(MCU_FLAGS) -T$(LDSCRIPT) \
            -specs=nano.specs -specs=nosys.specs \
            -Wl,-u,_printf_float \
            -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
            -Wl,--gc-sections -lc -lm -lnosys

##############################################################################
# Sources
#
# App/app_threadx.c, app_azure_rtos.c, app_filex.c and tx_initialize_low_level.c
# are shared kernel bring-up glue -- always built. Only App/app_$(APP)/*.c is
# app-specific and switches with APP=.
##############################################################################
C_SOURCES := \
  $(wildcard Core/Src/*.c) \
  $(wildcard Sensors/*.c Sensors/sensor_bgt_w87x/*.c) \
  $(wildcard Middlewares/modbus/*.c) \
  $(wildcard Middlewares/log/src/*.c) \
  $(wildcard Middlewares/esp_at/*.c) \
  App/app_threadx.c \
  App/app_azure_rtos.c \
  App/app_filex.c \
  App/tx_initialize_low_level.c \
  $(wildcard $(APP_DIR)/*.c) \
  $(wildcard Drivers/STM32F4xx_HAL_Driver/Src/*.c) \
  $(wildcard Middlewares/threadx/common/src/*.c)

# ThreadX Cortex-M4/GNU port -- exact file list from
# Middlewares/threadx/ports/cortex_m4/gnu/CMakeLists.txt (the upstream-blessed
# set; tx_misra.S and the standalone interrupt_disable/restore.S files are
# intentionally NOT part of this list).
ASM_SOURCES := \
  Core/Startup/startup_stm32f412rx.s \
  Middlewares/threadx/ports/cortex_m4/gnu/src/tx_thread_context_restore.S \
  Middlewares/threadx/ports/cortex_m4/gnu/src/tx_thread_context_save.S \
  Middlewares/threadx/ports/cortex_m4/gnu/src/tx_thread_interrupt_control.S \
  Middlewares/threadx/ports/cortex_m4/gnu/src/tx_thread_schedule.S \
  Middlewares/threadx/ports/cortex_m4/gnu/src/tx_thread_stack_build.S \
  Middlewares/threadx/ports/cortex_m4/gnu/src/tx_thread_system_return.S \
  Middlewares/threadx/ports/cortex_m4/gnu/src/tx_timer_interrupt.S

OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(patsubst %.S,%.o,$(ASM_SOURCES:.s=.o))))
DEPS    := $(OBJECTS:.o=.d)

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
vpath %.S $(sort $(dir $(ASM_SOURCES)))

##############################################################################
# Rules
##############################################################################
.PHONY: all flash clean clean-all list-apps print-size

all: $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin print-size

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(LDSCRIPT) | $(BUILD_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

print-size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

flash: all
	$(FLASH_CMD)

list-apps:
	@echo "Available apps:"
	@for d in App/app_*/; do echo "  $${d#App/app_}" | sed 's#/$$##'; done

clean:
	rm -rf $(BUILD_DIR)

clean-all:
	rm -rf build

-include $(DEPS)
