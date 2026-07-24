#include "bgt_w87x.h"
#include <stdint.h>


#define BGT_REG_TEMPERATURE  0x0000U 
#define BGT_REG_PRESSURE     0x0006U
#define BGT_REG_WIND_SPEED   0x000BU

#define BGT_NOT_MEASURED     0x7FFFU

static uint8_t slave_addr;

BGT_Status_e W87X_Init (uint8_t addr){
  slave_addr = addr;
  return BGT_OK;
}

static void W87X_LogFailure(const char *what, Modbus_StatusTypeDef status){
  LOG_ERROR("BGT-W87x: %s failed (status=%d, rxBytes=%u)",
            what, (int)status, (unsigned int)hmodbusSensor.rxLen);
}

BGT_Status_e W87X_GetData(BGT_Data_t *data){
  uint16_t regs[2];
  BGT_Status_e result = BGT_OK;
  Modbus_StatusTypeDef status;

  *data = (BGT_Data_t){0};

  status = Modbus_ReadHoldingRegisters(&hmodbusSensor, slave_addr,
                                        BGT_REG_TEMPERATURE, 2U, regs);
  if (status == MODBUS_OK){
    if (regs[0] != BGT_NOT_MEASURED){
      data->temperature = (int16_t)regs[0] * 0.1f;
    }
    if (regs[1] != BGT_NOT_MEASURED){
      data->humidity = (int16_t)regs[1] * 0.1f;
    }
  }else{
    W87X_LogFailure("Failed read sensor", status);
    result = BGT_ERR_MODBUS;
  }

  status = Modbus_ReadHoldingRegisters(&hmodbusSensor, slave_addr,
                                        BGT_REG_PRESSURE, 1U, regs);
  if (status == MODBUS_OK)
  {
    if (regs[0] != BGT_NOT_MEASURED)
    {
      data->pressure = (int16_t)regs[0] * 0.1f;
    }
  }
  else
  {
    W87X_LogFailure("Failed read pressure read", status);
    result = BGT_ERR_MODBUS;
  }

  status = Modbus_ReadHoldingRegisters(&hmodbusSensor, slave_addr,
                                        BGT_REG_WIND_SPEED, 2U, regs);
  if (status == MODBUS_OK)
  {
    if (regs[0] != BGT_NOT_MEASURED)
    {
      data->wind_speed = regs[0] * 0.01f; 
    }
    if (regs[1] != BGT_NOT_MEASURED)
    {
      data->wind_direction = regs[1]; 
    }
  }
  else
  {
    W87X_LogFailure("wind speed/direction read", status);
    result = BGT_ERR_MODBUS;
  }

  return result;
}
