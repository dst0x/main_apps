#ifndef __BGT_W87X_H
#define __BGT_W87X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "main.h"
#include "log.h"

typedef enum {
  BGT_OK = 0,
  BGT_ERR_MODBUS
} BGT_Status_e;

typedef struct {
  float temperature;
  float humidity;
  float pressure;
  float wind_speed;
  uint16_t wind_direction;
} BGT_Data_t;

BGT_Status_e W87X_Init(uint8_t addr);
BGT_Status_e W87X_GetData(BGT_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __BGT_W87X_H */
