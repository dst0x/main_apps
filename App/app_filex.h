/**
  ******************************************************************************
  * @file    app_filex.h
  * @brief   FileX init placeholder -- no storage device is wired up yet.
  *          app_azure_rtos.c calls this unconditionally; this stub just lets
  *          the project build/link until real FileX (SD card, USB, ...) is
  *          added. Replace the body of MX_FileX_Init() in app_filex.c when
  *          you actually need a filesystem.
  ******************************************************************************
  */

#ifndef APP_FILEX_H
#define APP_FILEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fx_api.h"

UINT MX_FileX_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* APP_FILEX_H */
