/**
  ******************************************************************************
  * @file    app_threadx.c
  * @brief   ThreadX bring-up hub: creates the selected app_xxx/ module's
  *          thread(s) and enters the kernel. Each app_xxx/ module (e.g.
  *          App/app_bgt/) is self-contained -- its own thread, its own
  *          sensor driver calls, nothing shared here.
  *
  *          Which app builds is chosen at compile time (see the Makefile's
  *          `APP=` variable, e.g. `make APP=bgt`), which passes a matching
  *          -DAPP_<NAME> define.
  *
  *          To add another app:
  *            1. Create App/app_yyy/app_yyy.{c,h} with its own
  *               App_YYY_Init(VOID *memory_ptr) that tx_thread_create()'s
  *               its thread (copy App/app_bgt/ as a starting point).
  *            2. Add an `#elif defined(APP_YYY)` branch below.
  *            3. `make APP=yyy` will now build it.
  ******************************************************************************
  */

#include "app_threadx.h"

volatile uint8_t g_ThreadXTickReady = 0U;

#if defined(APP_BGT)
#include "app_bgt/app_bgt.h"
#elif defined(APP_ESP32)
#include "app_esp32/app_esp32.h"
#else
#error "No app selected -- build with `make APP=<name>`, e.g. `make APP=bgt`"
#endif

UINT App_ThreadX_Init(VOID *memory_ptr)
{
#if defined(APP_BGT)
  return App_BGT_Init(memory_ptr);
#elif defined(APP_ESP32)
  return App_ESP32_Init(memory_ptr);
#endif
}

void MX_ThreadX_Init(void)
{
  tx_kernel_enter(); /* never returns */
}
