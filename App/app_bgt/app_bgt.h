#ifndef APP_BGT_H
#define APP_BGT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include "esp_at.h"

extern ESPAT_HandleTypeDef hEspAt;

UINT App_BGT_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* APP_BGT_H */
