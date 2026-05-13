#include "app_home_data.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

static APP_HomeDataTelemetry_t g_homeTelemetry[APP_HOME_DATA_ROOM_COUNT] = {0};

uint8_t APP_HomeData_NodeToRoomIndex(uint8_t node, uint8_t *roomIndex)
{
  uint8_t index = 0U;

  if ((node == 0U) || (node > APP_HOME_DATA_ROOM_COUNT) || (roomIndex == NULL))
  {
    return 0U;
  }

  index = (uint8_t)(node - 1U);
  *roomIndex = index;
  return 1U;
}

uint8_t APP_HomeData_UpdateTelemetry(uint8_t node,
                                     uint16_t sequence,
                                     int16_t temperature_x10,
                                     uint16_t humidity_x10,
                                     uint8_t mode,
                                     uint8_t fan,
                                     uint8_t online)
{
  uint8_t roomIndex = 0U;
  APP_HomeDataTelemetry_t telemetry = {0};

  if (APP_HomeData_NodeToRoomIndex(node, &roomIndex) == 0U)
  {
    return 0U;
  }

  telemetry.valid = 1U;
  telemetry.node = node;
  telemetry.sequence = sequence;
  telemetry.temperature_x10 = temperature_x10;
  telemetry.humidity_x10 = humidity_x10;
  telemetry.mode = mode;
  telemetry.fan = fan;
  telemetry.online = online;
  telemetry.updated_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

  taskENTER_CRITICAL();
  g_homeTelemetry[roomIndex] = telemetry;
  taskEXIT_CRITICAL();

  return 1U;
}

uint8_t APP_HomeData_CopyTelemetryByRoomIndex(uint8_t roomIndex,
                                              APP_HomeDataTelemetry_t *telemetry)
{
  if ((roomIndex >= APP_HOME_DATA_ROOM_COUNT) || (telemetry == NULL))
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  memcpy(telemetry, &g_homeTelemetry[roomIndex], sizeof(*telemetry));
  taskEXIT_CRITICAL();

  return telemetry->valid;
}
