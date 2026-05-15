#include "app_home_data.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

static APP_HomeDataTelemetry_t g_homeTelemetry[APP_HOME_DATA_ROOM_COUNT] = {0};
static APP_HomeDataNodeStatus_t g_homeNodeStatus[APP_HOME_DATA_ROOM_COUNT] = {0};

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
                                     uint8_t online,
                                     uint8_t output_flags)
{
  uint8_t roomIndex = 0U;
  APP_HomeDataTelemetry_t telemetry = {0};
  APP_HomeDataNodeStatus_t nodeStatus = {0};
  const uint32_t nowMs = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

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
  telemetry.output_flags = output_flags;
  telemetry.updated_ms = nowMs;

  nodeStatus.known = 1U;
  nodeStatus.online = online;
  nodeStatus.updated_ms = nowMs;

  taskENTER_CRITICAL();
  g_homeTelemetry[roomIndex] = telemetry;
  g_homeNodeStatus[roomIndex] = nodeStatus;
  taskEXIT_CRITICAL();

  return 1U;
}

uint8_t APP_HomeData_UpdateNodeOnline(uint8_t node, uint8_t online)
{
  uint8_t roomIndex = 0U;
  APP_HomeDataNodeStatus_t nodeStatus = {0};

  if (APP_HomeData_NodeToRoomIndex(node, &roomIndex) == 0U)
  {
    return 0U;
  }

  nodeStatus.known = 1U;
  nodeStatus.online = (online != 0U) ? 1U : 0U;
  nodeStatus.updated_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

  taskENTER_CRITICAL();
  g_homeNodeStatus[roomIndex] = nodeStatus;
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

uint8_t APP_HomeData_CopyNodeStatusByRoomIndex(uint8_t roomIndex,
                                               APP_HomeDataNodeStatus_t *status)
{
  if ((roomIndex >= APP_HOME_DATA_ROOM_COUNT) || (status == NULL))
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  memcpy(status, &g_homeNodeStatus[roomIndex], sizeof(*status));
  taskEXIT_CRITICAL();

  return status->known;
}
