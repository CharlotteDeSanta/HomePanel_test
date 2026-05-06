#ifndef APP_WIFI_LWIP_H
#define APP_WIFI_LWIP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void APP_WiFi_LwIP_Service(void);
void APP_WiFi_LwIP_ProcessEthernetFrame(const uint8_t *frame, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_LWIP_H */
