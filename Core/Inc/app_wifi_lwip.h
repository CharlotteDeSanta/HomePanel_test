#ifndef APP_WIFI_LWIP_H
#define APP_WIFI_LWIP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint16_t activeClients;
  uint16_t closePendingClients;
  uint16_t txQueueDepth;
  uint16_t pendingControls;
  uint16_t inflightControls;
  uint32_t taskCount;
  uint32_t freeHeapBytes;
  uint32_t minEverFreeHeapBytes;
  uint32_t txEnqueueFailCount;
  uint32_t rxPbufAllocFailCount;
  uint32_t rxTcpipInputFailCount;
} APP_WiFi_LwIP_RuntimeStats_t;

void APP_WiFi_LwIP_Service(void);
void APP_WiFi_LwIP_ProcessEthernetFrame(const uint8_t *frame, uint16_t length);
uint8_t APP_WiFi_LwIP_IsNetworkOnline(void);
uint8_t APP_WiFi_LwIP_SendControl(uint8_t node,
                                  int16_t targetTemperature_x10,
                                  uint8_t mode,
                                  uint8_t fan,
                                  uint8_t flags);
uint8_t APP_WiFi_LwIP_HasPendingTx(void);
uint32_t APP_WiFi_LwIP_GetRxEthernetFrameCount(void);
void APP_WiFi_LwIP_GetRuntimeStats(APP_WiFi_LwIP_RuntimeStats_t *stats);
void APP_WiFi_LwIP_RequestSessionRefresh(void);
void APP_WiFi_LwIP_RequestNetworkRebind(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_LWIP_H */
