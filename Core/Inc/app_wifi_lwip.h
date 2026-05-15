#ifndef APP_WIFI_LWIP_H
#define APP_WIFI_LWIP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void APP_WiFi_LwIP_Service(void);
void APP_WiFi_LwIP_ProcessEthernetFrame(const uint8_t *frame, uint16_t length);
uint8_t APP_WiFi_LwIP_SendControl(uint8_t node,
                                  int16_t targetTemperature_x10,
                                  uint8_t mode,
                                  uint8_t fan,
                                  uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_LWIP_H */
