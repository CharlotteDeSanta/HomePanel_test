#ifndef APP_HOME_DATA_H
#define APP_HOME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_HOME_DATA_ROOM_COUNT 3U

typedef struct
{
  uint8_t valid;
  uint8_t node;
  uint16_t sequence;
  int16_t temperature_x10;
  uint16_t humidity_x10;
  uint8_t mode;
  uint8_t fan;
  uint8_t online;
  uint8_t output_flags;
  uint32_t updated_ms;
} APP_HomeDataTelemetry_t;

uint8_t APP_HomeData_NodeToRoomIndex(uint8_t node, uint8_t *roomIndex);
uint8_t APP_HomeData_UpdateTelemetry(uint8_t node,
                                     uint16_t sequence,
                                     int16_t temperature_x10,
                                     uint16_t humidity_x10,
                                     uint8_t mode,
                                     uint8_t fan,
                                     uint8_t online,
                                     uint8_t output_flags);
uint8_t APP_HomeData_CopyTelemetryByRoomIndex(uint8_t roomIndex,
                                              APP_HomeDataTelemetry_t *telemetry);

#ifdef __cplusplus
}
#endif

#endif /* APP_HOME_DATA_H */
