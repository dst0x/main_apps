/**
  ******************************************************************************
  * @file    esp32c3.c
  * @brief   ESP32-C3 AT-command driver -- see esp32c3.h.
  ******************************************************************************
  */

#include "esp32c3.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
  * @brief  Extracts the (1-based) comma-separated field at `fieldIndex` from
  *         `line`, respecting double-quoted substrings (so a quoted SSID
  *         can't be mistaken for extra fields). Used to pull the RSSI out of
  *         AT+CWJAP?'s +CWJAP:"ssid","bssid",channel,rssi,... reply.
  */
static int ESP32_CommaField(const char *line, int fieldIndex, char *out, size_t outSize)
{
  int      field = 1;
  uint8_t  inQuotes = 0U;
  size_t   outPos = 0;
  const char *p = line;

  while (*p != '\0')
  {
    if (*p == '"')
    {
      inQuotes = (uint8_t)(inQuotes == 0U ? 1U : 0U);
      p++;
      continue;
    }

    if ((*p == ',') && (inQuotes == 0U))
    {
      field++;
      p++;
      continue;
    }

    if (field == fieldIndex)
    {
      if (outPos < (outSize - 1U))
      {
        out[outPos++] = *p;
      }
    }
    else if (field > fieldIndex)
    {
      break;
    }

    p++;
  }

  out[outPos] = '\0';
  return (outPos > 0U) ? 0 : -1;
}

#define ESP32_RESYNC_JUNK_LEN  400U /* comfortably longer than any raw payload this app ever sends (JSON payload buffer is 160 bytes) */

void ESP32_UartResync(ESPAT_HandleTypeDef *hat)
{
  uint8_t junk[ESP32_RESYNC_JUNK_LEN];

  if (hat == NULL)
  {
    return;
  }

  memset(junk, '\r', sizeof(junk));
  (void)HAL_UART_Transmit(hat->huart, junk, sizeof(junk), 1000U);
  HAL_Delay(200U);
}

ESP32_StatusTypeDef ESP32_Init(ESPAT_HandleTypeDef *hat)
{
  char resp[ESPAT_RESP_BUF_SIZE];
  ESPAT_StatusTypeDef atStatus;

  atStatus = ESPAT_SendCommand(hat, "AT", 2000U, resp, sizeof(resp));
  if (atStatus != ESPAT_OK)
  {
    /* txStatus/armStatus != 0 (HAL_OK) -> the STM32 side itself failed to
     * send or to start listening (peripheral/clock/NVIC config problem, not
     * wiring). Both 0 but rxBytes=0 -> STM32 side is fine, genuinely nothing
     * came back on USART6 RX -- check PC6/PC7 wiring (TX/RX must be
     * crossed), common GND, module power, and that the module really is at
     * 115200 baud. rxBytes>0 but still failing -> something answered but
     * wasn't "...OK\r\n"; see "resp" (garbled text usually means wrong baud). */
    LOG_ERROR("ESP32: \"AT\" got no valid reply (atStatus=%d, txStatus=%d, armStatus=%d, "
              "rxBytes=%u, resp=\"%s\")",
              (int)atStatus, (int)hat->lastTxStatus, (int)hat->lastArmStatus,
              (unsigned int)hat->respLen, resp);
    return ESP32_ERR_TIMEOUT;
  }
  if (ESPAT_SendCommand(hat, "ATE0", 2000U, NULL, 0U) != ESPAT_OK) /* echo off */
  {
    return ESP32_ERR_TIMEOUT;
  }
  if (ESPAT_SendCommand(hat, "AT+CWMODE=1", 2000U, NULL, 0U) != ESPAT_OK) /* station mode */
  {
    return ESP32_ERR_REPLY;
  }
  /* Set up SNTP once so ESP32_GetTime() can just query it later. GMT+7. */
  (void)ESPAT_SendCommand(hat, "AT+CIPSNTPCFG=1,7,\"pool.ntp.org\"", 5000U, NULL, 0U);

  return ESP32_OK;
}

ESP32_StatusTypeDef ESP32_ConnectWiFi(ESPAT_HandleTypeDef *hat, const char *ssid,
                                       const char *password, uint32_t timeoutMs)
{
  char cmd[300];
  int  len;

  if ((hat == NULL) || (ssid == NULL) || (password == NULL))
  {
    return ESP32_ERR_PARAM;
  }

  len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
  if ((len <= 0) || ((size_t)len >= sizeof(cmd)))
  {
    return ESP32_ERR_PARAM;
  }

  switch (ESPAT_SendCommand(hat, cmd, timeoutMs, NULL, 0U))
  {
    case ESPAT_OK:      return ESP32_OK;
    case ESPAT_ERR_REPLY: return ESP32_ERR_REPLY;
    default:            return ESP32_ERR_TIMEOUT;
  }
}

ESP32_StatusTypeDef ESP32_GetTime(ESPAT_HandleTypeDef *hat, char *outTimeStr, uint16_t outSize)
{
  char   resp[ESPAT_RESP_BUF_SIZE];
  char  *p;
  char  *lineEnd;

  if ((hat == NULL) || (outTimeStr == NULL) || (outSize == 0U))
  {
    return ESP32_ERR_PARAM;
  }

  if (ESPAT_SendCommand(hat, "AT+CIPSNTPTIME?", 3000U, resp, sizeof(resp)) != ESPAT_OK)
  {
    return ESP32_ERR_TIMEOUT;
  }

  p = strstr(resp, "+CIPSNTPTIME:");
  if (p == NULL)
  {
    return ESP32_ERR_PARSE;
  }
  p += strlen("+CIPSNTPTIME:");

  lineEnd = strstr(p, "\r\n");
  {
    size_t copyLen = (lineEnd != NULL) ? (size_t)(lineEnd - p) : strlen(p);
    if (copyLen >= outSize)
    {
      copyLen = outSize - 1U;
    }
    memcpy(outTimeStr, p, copyLen);
    outTimeStr[copyLen] = '\0';
  }

  return ESP32_OK;
}

/**
  * @brief  Days since 1970-01-01 for a given proleptic-Gregorian civil date
  *         (Howard Hinnant's days_from_civil). Used instead of mktime()/
  *         timegm() so the conversion needs no TZ database and can't pick up
  *         the host's local timezone -- there isn't one configured here.
  */
static long ESP32_DaysFromCivil(int y, int m, int d)
{
  long     era;
  unsigned yoe;
  unsigned doy;
  unsigned doe;

  y -= (m <= 2) ? 1 : 0;
  era = ((y >= 0) ? y : (y - 399)) / 400;
  yoe = (unsigned)(y - (era * 400));
  doy = (unsigned)(((153 * (m + ((m > 2) ? -3 : 9))) + 2) / 5 + d - 1);
  doe = (yoe * 365U) + (yoe / 4U) - (yoe / 100U) + doy;

  return (era * 146097L) + (long)doe - 719468L;
}

ESP32_StatusTypeDef ESP32_ParseTime(const char *timeStr, time_t *outEpoch)
{
  static const char *const kMonths[12] =
  {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  char monStr[4] = { 0 };
  int  day, hh, mm, ss, year;
  int  month = -1;
  int  i;

  if ((timeStr == NULL) || (outEpoch == NULL))
  {
    return ESP32_ERR_PARAM;
  }

  if (sscanf(timeStr, "%*s %3s %d %d:%d:%d %d", monStr, &day, &hh, &mm, &ss, &year) != 6)
  {
    return ESP32_ERR_PARSE;
  }

  for (i = 0; i < 12; i++)
  {
    if (strncmp(monStr, kMonths[i], 3) == 0)
    {
      month = i + 1;
      break;
    }
  }
  if (month < 0)
  {
    return ESP32_ERR_PARSE;
  }

  *outEpoch = (time_t)((ESP32_DaysFromCivil(year, month, day) * 86400L)
                        + (hh * 3600) + (mm * 60) + ss);
  return ESP32_OK;
}

ESP32_StatusTypeDef ESP32_GetRSSI(ESPAT_HandleTypeDef *hat, int16_t *outRssiDbm)
{
  char  resp[ESPAT_RESP_BUF_SIZE];
  char *p;
  char  field[16];

  if ((hat == NULL) || (outRssiDbm == NULL))
  {
    return ESP32_ERR_PARAM;
  }

  if (ESPAT_SendCommand(hat, "AT+CWJAP?", 3000U, resp, sizeof(resp)) != ESPAT_OK)
  {
    return ESP32_ERR_TIMEOUT;
  }

  p = strstr(resp, "+CWJAP:");
  if (p == NULL)
  {
    return ESP32_ERR_PARSE; /* not currently associated to an AP */
  }
  p += strlen("+CWJAP:");

  /* +CWJAP:"ssid","bssid",channel,rssi,... -- rssi is field 4 */
  if (ESP32_CommaField(p, 4, field, sizeof(field)) != 0)
  {
    return ESP32_ERR_PARSE;
  }

  *outRssiDbm = (int16_t)strtol(field, NULL, 10);
  return ESP32_OK;
}

ESP32_StatusTypeDef ESP32_MQTT_Connect(ESPAT_HandleTypeDef *hat, const char *clientId,
                                        const char *username, const char *password,
                                        const char *host, uint16_t port, uint32_t timeoutMs)
{
  char cmd[300];
  int  len;

  if ((hat == NULL) || (clientId == NULL) || (username == NULL) || (password == NULL)
      || (host == NULL))
  {
    return ESP32_ERR_PARAM;
  }

  /* Best-effort: drop any pre-existing session on link 0. If the MCU alone
   * reset (WDG, brownout, ...) the ESP32-C3 module itself never power-cycled
   * and may still consider itself connected from before -- AT+MQTTCONN on an
   * already-connected link just returns ERROR forever, wedging the retry
   * loop below permanently. Ignore the result: this harmlessly fails if
   * link 0 wasn't connected to begin with. */
  (void)ESPAT_SendCommand(hat, "AT+MQTTCLEAN=0", 5000U, NULL, 0U);

  /* LinkID 0, scheme 1 = MQTT over plain TCP, login via username/password, no cert. */
  len = snprintf(cmd, sizeof(cmd), "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
                 clientId, username, password);
  if ((len <= 0) || ((size_t)len >= sizeof(cmd)))
  {
    return ESP32_ERR_PARAM;
  }
  if (ESPAT_SendCommand(hat, cmd, 5000U, NULL, 0U) != ESPAT_OK)
  {
    return ESP32_ERR_REPLY;
  }

  len = snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=0,\"%s\",%u,1", host, (unsigned int)port);
  if ((len <= 0) || ((size_t)len >= sizeof(cmd)))
  {
    return ESP32_ERR_PARAM;
  }

  switch (ESPAT_SendCommand(hat, cmd, timeoutMs, NULL, 0U))
  {
    case ESPAT_OK:        return ESP32_OK;
    case ESPAT_ERR_REPLY: return ESP32_ERR_REPLY;
    default:              return ESP32_ERR_TIMEOUT;
  }
}

/**
  * @brief  Parses a captured response buffer containing a
  *         "+MQTTSUBRECV:<link_id>,\"<topic>\",<data_length>,<data>" line
  *         where <data> is JSON of the form {"interval":<seconds>}. <data> is
  *         raw/unescaped, exactly <data_length> bytes -- NOT a quote-aware
  *         comma field (it legally contains commas, being JSON), so it can't
  *         go through ESP32_CommaField(). Uses %n to find exactly where
  *         <data> starts, then copies dataLen bytes by hand.
  */
static ESP32_StatusTypeDef ParseMqttSubRecvInterval(const char *resp, int32_t *outSeconds)
{
  const char *p;
  int          linkId;
  int          dataLen;
  int          consumed;

  p = strstr(resp, "+MQTTSUBRECV:");
  if (p == NULL)
  {
    return ESP32_ERR_PARSE;
  }
  p += strlen("+MQTTSUBRECV:");

  consumed = 0;
  if ((sscanf(p, "%d,\"%*[^\"]\",%d,%n", &linkId, &dataLen, &consumed) < 2) || (consumed == 0))
  {
    return ESP32_ERR_PARSE;
  }
  (void)linkId;

  if ((dataLen <= 0) || (dataLen > 63))
  {
    return ESP32_ERR_PARSE; /* not a plausible {"interval":<n>} payload */
  }

  {
    char  jsonBuf[64];
    char *keyPos;
    char *colonPos;
    char *end;
    long  val;

    memcpy(jsonBuf, p + consumed, (size_t)dataLen);
    jsonBuf[dataLen] = '\0';

    keyPos = strstr(jsonBuf, "\"interval\"");
    if (keyPos == NULL)
    {
      return ESP32_ERR_PARSE;
    }

    colonPos = strchr(keyPos, ':');
    if (colonPos == NULL)
    {
      return ESP32_ERR_PARSE;
    }

    val = strtol(colonPos + 1, &end, 10);
    if (end == (colonPos + 1))
    {
      return ESP32_ERR_PARSE; /* no digits after the colon */
    }

    *outSeconds = (int32_t)val;
  }

  return ESP32_OK;
}

ESP32_StatusTypeDef ESP32_MQTT_SubscribeAndGetInterval(ESPAT_HandleTypeDef *hat, const char *topic,
                                                        uint8_t qos, uint32_t subTimeoutMs,
                                                        uint32_t pushTimeoutMs, int32_t *outSeconds)
{
  char cmd[300];
  char resp[ESPAT_RESP_BUF_SIZE];
  int  len;

  if ((hat == NULL) || (topic == NULL) || (outSeconds == NULL))
  {
    return ESP32_ERR_PARAM;
  }

  len = snprintf(cmd, sizeof(cmd), "AT+MQTTSUB=0,\"%s\",%u", topic, (unsigned int)qos);
  if ((len <= 0) || ((size_t)len >= sizeof(cmd)))
  {
    return ESP32_ERR_PARAM;
  }

  {
    ESPAT_StatusTypeDef subStatus = ESPAT_SendCommand(hat, cmd, subTimeoutMs, resp, sizeof(resp));

    if (subStatus != ESPAT_OK)
    {
      LOG_WARNING("MQTT: %s failed (status=%d, resp=\"%s\")", cmd, (int)subStatus, resp);
      return (subStatus == ESPAT_ERR_REPLY) ? ESP32_ERR_REPLY : ESP32_ERR_TIMEOUT;
    }
  }

  /* A retained message can arrive before, interleaved with, or right after
   * the SUBACK's own OK -- brokers typically push it immediately on
   * subscribe, well within the same response window. Check what the
   * subscribe command itself already captured before waiting for more. */
  if (strstr(resp, "+MQTTSUBRECV:") != NULL)
  {
    ESP32_StatusTypeDef parseStatus = ParseMqttSubRecvInterval(resp, outSeconds);
    if (parseStatus != ESP32_OK)
    {
      LOG_WARNING("MQTT: got a push with the subscribe ACK but couldn't parse it: \"%s\"", resp);
    }
    return parseStatus;
  }

  LOG_INFO("MQTT: subscribed to %s (resp=\"%s\"), waiting up to %u ms for a retained push...",
           topic, resp, (unsigned int)pushTimeoutMs);

  if (ESPAT_WaitUnsolicited(hat, "+MQTTSUBRECV:", pushTimeoutMs, resp, sizeof(resp)) != ESPAT_OK)
  {
    LOG_WARNING("MQTT: no push arrived on %s within %u ms (not retained, or nothing published yet?)",
                topic, (unsigned int)pushTimeoutMs);
    return ESP32_ERR_TIMEOUT;
  }

  {
    ESP32_StatusTypeDef parseStatus = ParseMqttSubRecvInterval(resp, outSeconds);
    if (parseStatus != ESP32_OK)
    {
      LOG_WARNING("MQTT: push arrived but couldn't parse it: \"%s\"", resp);
    }
    return parseStatus;
  }
}

ESP32_StatusTypeDef ESP32_MQTT_Publish(ESPAT_HandleTypeDef *hat, const char *topic,
                                        const char *payload, uint8_t qos, uint8_t retain,
                                        uint32_t timeoutMs)
{
  char   cmd[ESPAT_CMD_BUF_SIZE - 16U]; /* leave room for ESPAT_SendCommand's own "\r\n" */
  int    len;
  size_t payloadLen;

  if ((hat == NULL) || (topic == NULL) || (payload == NULL))
  {
    return ESP32_ERR_PARAM;
  }

  payloadLen = strlen(payload);

  /* MQTTPUBRAW, not MQTTPUB: our payload is JSON, so it's full of '"' and
   * ',' -- MQTTPUB packs the payload as just another quoted AT argument and
   * has no escaping for those, so a JSON body corrupts the argument list.
   * MQTTPUBRAW instead declares an exact byte length and takes the payload
   * as an opaque raw-byte phase after the '>' prompt, so its content never
   * touches the AT argument parser. */
  len = snprintf(cmd, sizeof(cmd), "AT+MQTTPUBRAW=0,\"%s\",%u,%u,%u",
                 topic, (unsigned int)payloadLen, (unsigned int)qos, (unsigned int)retain);
  if ((len <= 0) || ((size_t)len >= sizeof(cmd)))
  {
    return ESP32_ERR_PARAM;
  }

  if (ESPAT_SendCommand(hat, cmd, timeoutMs, NULL, 0U) != ESPAT_OK)
  {
    return ESP32_ERR_REPLY;
  }

  switch (ESPAT_SendRaw(hat, (const uint8_t *)payload, (uint16_t)payloadLen, timeoutMs, NULL, 0U))
  {
    case ESPAT_OK:        return ESP32_OK;
    case ESPAT_ERR_REPLY: return ESP32_ERR_REPLY;
    default:              return ESP32_ERR_TIMEOUT;
  }
}
