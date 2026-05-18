#include "app_wifi_lwip.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/dhcp.h"
#include "lwip/err.h"
#include "lwip/etharp.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/pbuf.h"
#include "lwip/sockets.h"
#include "lwip/tcpip.h"

#include "app_home_data.h"
#include "app_home_protocol.h"
#include "app_wifi.h"

#define APP_WIFI_LWIP_TCP_SERVER_PORT      5000U
#define APP_WIFI_LWIP_TX_QUEUE_DEPTH        16U
#define APP_WIFI_LWIP_TX_BURST_LIMIT        8U
#define APP_WIFI_LWIP_TX_FLUSH_LOCK_WAIT_MS 10U
#define APP_WIFI_LWIP_CONTROL_ROOM_COUNT    APP_HOME_DATA_ROOM_COUNT
#define APP_WIFI_LWIP_CONTROL_POLL_MS       5U
#define APP_WIFI_LWIP_CONTROL_ACK_TIMEOUT_MS 20000U
#define APP_WIFI_LWIP_CONTROL_TX_GAP_MS     1500U
#define APP_WIFI_LWIP_CONTROL_DEBOUNCE_MS   1500U
#define APP_WIFI_LWIP_ARP_REFRESH_MS        5000U
#define APP_WIFI_LWIP_DHCP_WAIT_LOG_MS      5000U
#define APP_WIFI_LWIP_DHCP_RESTART_MS       15000U
#define APP_WIFI_LWIP_ETH_TRACE_ENABLE      0U
#define APP_WIFI_LWIP_SOCKET_STACK_BYTES    4096U
#define APP_WIFI_LWIP_CLIENT_STACK_BYTES    4096U
#define APP_WIFI_LWIP_SOCKET_PRIORITY       5U
#define APP_WIFI_LWIP_RX_LOG_LIMIT          0U
#define APP_WIFI_LWIP_VERBOSE_PROTO_LOG     0U

typedef struct
{
  uint8_t *data;
  uint16_t length;
} APP_WiFi_LwIP_TxPacket_t;

typedef struct
{
  uint8_t node;
  uint16_t sequence;
  int16_t targetTemperature_x10;
  uint8_t mode;
  uint8_t fan;
  uint8_t flags;
} APP_WiFi_LwIP_ControlCommand_t;

typedef struct
{
  int socketHandle;
} APP_WiFi_LwIP_ClientContext_t;

static struct netif g_wifiNetif = {0};
static SemaphoreHandle_t g_tcpipInitSemaphore = NULL;
static SemaphoreHandle_t g_txQueueMutex = NULL;
static SemaphoreHandle_t g_txFlushMutex = NULL;
static SemaphoreHandle_t g_controlMutex = NULL;
static APP_WiFi_LwIP_TxPacket_t g_txQueue[APP_WIFI_LWIP_TX_QUEUE_DEPTH] = {0};
static APP_WiFi_LwIP_ControlCommand_t g_pendingControls[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static APP_WiFi_LwIP_ControlCommand_t g_inflightControls[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static uint8_t g_pendingControlValid[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static uint8_t g_inflightControlValid[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static uint8_t g_txQueueHead = 0U;
static uint8_t g_txQueueTail = 0U;
static uint8_t g_txQueueCount = 0U;
static uint16_t g_controlSequence = 1U;
static TickType_t g_pendingControlReadyTick[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static TickType_t g_inflightControlTxTick[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static TickType_t g_nextControlTxTick[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static uint8_t g_tcpipStarted = 0U;
static uint8_t g_netifAdded = 0U;
static uint8_t g_netifUp = 0U;
static uint8_t g_netifMacSynced = 0U;
static uint8_t g_dhcpStarted = 0U;
static uint8_t g_ipAnnounced = 0U;
static uint8_t g_peerProtocolSeen = 0U;
static uint8_t g_serverStarted = 0U;
static uint8_t g_lwipReadyLogged = 0U;
static TickType_t g_nextArpRefreshTick = 0U;
static TickType_t g_dhcpStartTick = 0U;
static TickType_t g_nextDhcpWaitLogTick = 0U;
static uint32_t g_linkOutputCount = 0U;
static uint32_t g_txOkCount = 0U;
static uint32_t g_txBusyCount = 0U;
static uint32_t g_txErrorCount = 0U;
static uint32_t g_arpRefreshCount = 0U;
static uint32_t g_netifRxLogCount = 0U;
static uint32_t g_serverRxCount = 0U;
static uint32_t g_serverFrameCount = 0U;
static uint32_t g_serverAckCount = 0U;
static uint32_t g_serverErrCount = 0U;
static uint32_t g_serverControlQueuedCount = 0U;
static uint32_t g_serverControlTxCount = 0U;
static uint32_t g_serverControlAckCount = 0U;
static uint32_t g_serverControlErrCount = 0U;
static uint32_t g_serverControlTimeoutCount = 0U;
static uint32_t g_serverSendErrorCount = 0U;
static uint32_t g_dhcpRestartCount = 0U;

static void APP_WiFi_LwIP_TcpipInitDone(void *arg);
static err_t APP_WiFi_LwIP_NetifInit(struct netif *netif);
static err_t APP_WiFi_LwIP_LinkOutput(struct netif *netif, struct pbuf *p);
static uint8_t APP_WiFi_LwIP_QueuePush(uint8_t *data, uint16_t length);
static uint8_t APP_WiFi_LwIP_QueuePeek(APP_WiFi_LwIP_TxPacket_t *packet);
static void APP_WiFi_LwIP_QueuePop(void);
static void APP_WiFi_LwIP_DropQueuedTx(void);
static void APP_WiFi_LwIP_FlushTxQueue(void);
static void APP_WiFi_LwIP_LogRxFrame(const uint8_t *frame, uint16_t length);
static void APP_WiFi_LwIP_LogTxFrame(const uint8_t *frame, uint16_t length, uint32_t packetIndex);
static void APP_WiFi_LwIP_LogBufferHex(const char *prefix, const uint8_t *data, uint16_t length);
static void APP_WiFi_LwIP_StartServerTask(void);
static void APP_WiFi_LwIP_ServerTask(void *argument);
static void APP_WiFi_LwIP_ClientTask(void *argument);
static void APP_WiFi_LwIP_EnsureInitialized(void);
static err_t APP_WiFi_LwIP_RefreshNetifMacAddress(struct netif *netif);
static err_t APP_WiFi_LwIP_SendGratuitousArp(struct netif *netif);
static void APP_WiFi_LwIP_MaybeRefreshArp(void);
static void APP_WiFi_LwIP_StartDhcp(void);
static void APP_WiFi_LwIP_CheckDhcpProgress(void);
static uint8_t APP_WiFi_LwIP_EnsureControlMutex(void);
static void APP_WiFi_LwIP_ResetControlState(void);
static uint8_t APP_WiFi_LwIP_StorePendingControl(const APP_WiFi_LwIP_ControlCommand_t *command);
static uint8_t APP_WiFi_LwIP_IsSameControlPayload(const APP_WiFi_LwIP_ControlCommand_t *lhs,
                                                  const APP_WiFi_LwIP_ControlCommand_t *rhs);
static uint8_t APP_WiFi_LwIP_SendAll(int socketHandle, const uint8_t *data, uint16_t length);
static uint8_t APP_WiFi_LwIP_SendControlFrame(int clientSocket, const APP_WiFi_LwIP_ControlCommand_t *command);
static void APP_WiFi_LwIP_DrainPendingControls(int clientSocket, uint8_t clientNode);
static void APP_WiFi_LwIP_HandleProtocolFrame(int clientSocket, const APP_HomeProtocolFrame_t *frame, uint8_t *clientNode);
static void APP_WiFi_LwIP_HandleControlReply(const APP_HomeProtocolFrame_t *frame, uint8_t isError);
static void APP_WiFi_LwIP_RequeueInflightControl(uint8_t clientNode);
static void APP_WiFi_LwIP_SendProtocolAck(int clientSocket, const APP_HomeProtocolFrame_t *frame);
static void APP_WiFi_LwIP_SendProtocolError(int clientSocket,
                                            uint8_t node,
                                            uint16_t sequence,
                                            uint8_t command,
                                            APP_HomeProtocolError_t error);
static uint8_t APP_WiFi_LwIP_IsKnownProtocolCommand(uint8_t command);
static uint8_t APP_WiFi_LwIP_IsKnownNode(uint8_t node);
static uint16_t APP_WiFi_LwIP_ReadLe16(const uint8_t *data);
static int16_t APP_WiFi_LwIP_ReadI16(const uint8_t *data);

static void APP_WiFi_LwIP_TcpipInitDone(void *arg)
{
  SemaphoreHandle_t *doneSemaphore = (SemaphoreHandle_t *)arg;

  if (doneSemaphore != NULL)
  {
    (void)xSemaphoreGive(*doneSemaphore);
  }
}

static err_t APP_WiFi_LwIP_LinkOutput(struct netif *netif, struct pbuf *p)
{
  uint8_t *copy = NULL;
  uint16_t packetLength = 0U;
  uint16_t bytesCopied = 0U;

  (void)netif;

  if (p == NULL)
  {
    return ERR_ARG;
  }

  packetLength = p->tot_len;
  if ((packetLength == 0U) || (packetLength > APP_WIFI_LWIP_TX_PACKET_MAX_LEN))
  {
    return ERR_VAL;
  }

  copy = (uint8_t *)pvPortMalloc(packetLength);
  if (copy == NULL)
  {
    return ERR_MEM;
  }

  bytesCopied = pbuf_copy_partial(p, copy, packetLength, 0U);
  if (bytesCopied != packetLength)
  {
    vPortFree(copy);
    return ERR_VAL;
  }

  if (APP_WiFi_LwIP_QueuePush(copy, packetLength) == 0U)
  {
    vPortFree(copy);
    printf("[lwip] tx enqueue failed len=%u\n", (unsigned int)packetLength);
    return ERR_MEM;
  }

  g_linkOutputCount++;
  if ((APP_WIFI_LWIP_ETH_TRACE_ENABLE != 0U) && (g_linkOutputCount <= 32U))
  {
    printf("[lwip] linkoutput #%lu len=%u type=0x%02X%02X q=%u\n",
           (unsigned long)g_linkOutputCount,
           (unsigned int)packetLength,
           (packetLength >= 13U) ? (unsigned int)copy[12] : 0U,
           (packetLength >= 14U) ? (unsigned int)copy[13] : 0U,
           (unsigned int)g_txQueueCount);
    APP_WiFi_LwIP_LogTxFrame(copy, packetLength, g_linkOutputCount);
  }

  /* Keep SDIO TX serialized in the WiFi task; linkoutput may run in tcpip_thread. */
  return ERR_OK;
}

static err_t APP_WiFi_LwIP_NetifInit(struct netif *netif)
{
  uint8_t macAddress[APP_WIFI_MAC_ADDRESS_SIZE] = {0U};

  if (netif == NULL)
  {
    return ERR_ARG;
  }

  (void)APP_WiFi_GetMacAddress(macAddress);

  netif->name[0] = 'w';
  netif->name[1] = 'l';
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
  memcpy(netif->hwaddr, macAddress, APP_WIFI_MAC_ADDRESS_SIZE);
  netif->mtu = 1500U;
  netif->flags = (u8_t)(NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET);
  netif->output = etharp_output;
  netif->linkoutput = APP_WiFi_LwIP_LinkOutput;

  return ERR_OK;
}

static err_t APP_WiFi_LwIP_RefreshNetifMacAddress(struct netif *netif)
{
  uint8_t macAddress[APP_WIFI_MAC_ADDRESS_SIZE] = {0U};

  if (netif == NULL)
  {
    return ERR_ARG;
  }

  if (APP_WiFi_GetMacAddress(macAddress) == 0U)
  {
    return ERR_IF;
  }

  if (memcmp(netif->hwaddr, macAddress, APP_WIFI_MAC_ADDRESS_SIZE) != 0)
  {
    memcpy(netif->hwaddr, macAddress, APP_WIFI_MAC_ADDRESS_SIZE);
  }

  printf("[lwip] netif mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
         (unsigned int)macAddress[0],
         (unsigned int)macAddress[1],
         (unsigned int)macAddress[2],
         (unsigned int)macAddress[3],
         (unsigned int)macAddress[4],
         (unsigned int)macAddress[5]);

  return ERR_OK;
}

static err_t APP_WiFi_LwIP_SendGratuitousArp(struct netif *netif)
{
  if (netif == NULL)
  {
    return ERR_ARG;
  }

  return etharp_gratuitous(netif);
}

static void APP_WiFi_LwIP_MaybeRefreshArp(void)
{
  TickType_t nowTick = xTaskGetTickCount();
  err_t result = ERR_OK;

  if ((g_netifUp == 0U) ||
      (dhcp_supplied_address(&g_wifiNetif) == 0) ||
      (g_peerProtocolSeen != 0U) ||
      ((g_nextArpRefreshTick != 0U) &&
       ((int32_t)(nowTick - g_nextArpRefreshTick) < 0)))
  {
    return;
  }

  result = netifapi_netif_common(&g_wifiNetif, NULL, APP_WiFi_LwIP_SendGratuitousArp);
  g_nextArpRefreshTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_ARP_REFRESH_MS);
  if (result == ERR_OK)
  {
    g_arpRefreshCount++;
    if ((g_arpRefreshCount <= 20U) || ((g_arpRefreshCount % 12U) == 0U))
    {
      printf("[lwip] gratuitous arp #%lu waiting for peer\n", (unsigned long)g_arpRefreshCount);
    }
  }
  else
  {
    printf("[lwip] gratuitous arp failed: %d\n", (int)result);
  }
}

static void APP_WiFi_LwIP_StartDhcp(void)
{
  if (netifapi_dhcp_start(&g_wifiNetif) == ERR_OK)
  {
    const TickType_t nowTick = xTaskGetTickCount();

    g_dhcpStarted = 1U;
    g_dhcpStartTick = nowTick;
    g_nextDhcpWaitLogTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_DHCP_WAIT_LOG_MS);
    printf("[lwip] dhcp start\n");
  }
  else
  {
    printf("[lwip] dhcp start failed\n");
  }
}

static void APP_WiFi_LwIP_CheckDhcpProgress(void)
{
  const TickType_t nowTick = xTaskGetTickCount();
  const uint32_t elapsedMs = (uint32_t)((nowTick - g_dhcpStartTick) * portTICK_PERIOD_MS);

  if ((g_dhcpStarted == 0U) || (dhcp_supplied_address(&g_wifiNetif) != 0))
  {
    return;
  }

  if ((int32_t)(nowTick - g_nextDhcpWaitLogTick) >= 0)
  {
    printf("[lwip] dhcp waiting %lums tx=%lu ok=%lu busy=%lu err=%lu q=%u\n",
           (unsigned long)elapsedMs,
           (unsigned long)g_linkOutputCount,
           (unsigned long)g_txOkCount,
           (unsigned long)g_txBusyCount,
           (unsigned long)g_txErrorCount,
           (unsigned int)g_txQueueCount);
    g_nextDhcpWaitLogTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_DHCP_WAIT_LOG_MS);
  }

  if (elapsedMs >= APP_WIFI_LWIP_DHCP_RESTART_MS)
  {
    g_dhcpRestartCount++;
    printf("[lwip] dhcp restart #%lu tx=%lu ok=%lu busy=%lu err=%lu q=%u\n",
           (unsigned long)g_dhcpRestartCount,
           (unsigned long)g_linkOutputCount,
           (unsigned long)g_txOkCount,
           (unsigned long)g_txBusyCount,
           (unsigned long)g_txErrorCount,
           (unsigned int)g_txQueueCount);

    (void)netifapi_dhcp_stop(&g_wifiNetif);
    g_dhcpStarted = 0U;
    g_ipAnnounced = 0U;
    APP_WiFi_LwIP_DropQueuedTx();
    APP_WiFi_LwIP_StartDhcp();
  }
}

static uint8_t APP_WiFi_LwIP_QueuePush(uint8_t *data, uint16_t length)
{
  if ((data == NULL) || (g_txQueueMutex == NULL))
  {
    return 0U;
  }

  if (xSemaphoreTake(g_txQueueMutex, portMAX_DELAY) != pdTRUE)
  {
    return 0U;
  }

  if (g_txQueueCount >= APP_WIFI_LWIP_TX_QUEUE_DEPTH)
  {
    (void)xSemaphoreGive(g_txQueueMutex);
    return 0U;
  }

  g_txQueue[g_txQueueTail].data = data;
  g_txQueue[g_txQueueTail].length = length;
  g_txQueueTail = (uint8_t)((g_txQueueTail + 1U) % APP_WIFI_LWIP_TX_QUEUE_DEPTH);
  g_txQueueCount++;

  (void)xSemaphoreGive(g_txQueueMutex);
  return 1U;
}

static uint8_t APP_WiFi_LwIP_QueuePeek(APP_WiFi_LwIP_TxPacket_t *packet)
{
  if ((packet == NULL) || (g_txQueueMutex == NULL))
  {
    return 0U;
  }

  if (xSemaphoreTake(g_txQueueMutex, portMAX_DELAY) != pdTRUE)
  {
    return 0U;
  }

  if (g_txQueueCount == 0U)
  {
    (void)xSemaphoreGive(g_txQueueMutex);
    return 0U;
  }

  *packet = g_txQueue[g_txQueueHead];
  (void)xSemaphoreGive(g_txQueueMutex);
  return 1U;
}

static void APP_WiFi_LwIP_QueuePop(void)
{
  if (g_txQueueMutex == NULL)
  {
    return;
  }

  if (xSemaphoreTake(g_txQueueMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  if (g_txQueueCount != 0U)
  {
    g_txQueue[g_txQueueHead].data = NULL;
    g_txQueue[g_txQueueHead].length = 0U;
    g_txQueueHead = (uint8_t)((g_txQueueHead + 1U) % APP_WIFI_LWIP_TX_QUEUE_DEPTH);
    g_txQueueCount--;
  }

  (void)xSemaphoreGive(g_txQueueMutex);
}

static uint8_t APP_WiFi_LwIP_EnsureControlMutex(void)
{
  if (g_controlMutex != NULL)
  {
    return 1U;
  }

  g_controlMutex = xSemaphoreCreateMutex();
  return (g_controlMutex != NULL) ? 1U : 0U;
}

static void APP_WiFi_LwIP_ResetControlState(void)
{
  if ((APP_WiFi_LwIP_EnsureControlMutex() == 0U) ||
      (xSemaphoreTake(g_controlMutex, portMAX_DELAY) != pdTRUE))
  {
    return;
  }

  memset(g_pendingControls, 0, sizeof(g_pendingControls));
  memset(g_inflightControls, 0, sizeof(g_inflightControls));
  memset(g_pendingControlValid, 0, sizeof(g_pendingControlValid));
  memset(g_inflightControlValid, 0, sizeof(g_inflightControlValid));
  memset(g_pendingControlReadyTick, 0, sizeof(g_pendingControlReadyTick));
  memset(g_inflightControlTxTick, 0, sizeof(g_inflightControlTxTick));
  memset(g_nextControlTxTick, 0, sizeof(g_nextControlTxTick));

  (void)xSemaphoreGive(g_controlMutex);
}

static uint8_t APP_WiFi_LwIP_StorePendingControl(const APP_WiFi_LwIP_ControlCommand_t *command)
{
  uint8_t roomIndex = 0U;
  TickType_t readyTick = xTaskGetTickCount() + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_DEBOUNCE_MS);

  if ((command == NULL) ||
      (APP_HomeData_NodeToRoomIndex(command->node, &roomIndex) == 0U) ||
      (APP_WiFi_LwIP_EnsureControlMutex() == 0U))
  {
    return 0U;
  }

  if (xSemaphoreTake(g_controlMutex, portMAX_DELAY) != pdTRUE)
  {
    return 0U;
  }

  g_pendingControls[roomIndex] = *command;
  g_pendingControlValid[roomIndex] = 1U;
  g_pendingControlReadyTick[roomIndex] = readyTick;

  (void)xSemaphoreGive(g_controlMutex);
  return 1U;
}

static uint8_t APP_WiFi_LwIP_IsSameControlPayload(const APP_WiFi_LwIP_ControlCommand_t *lhs,
                                                  const APP_WiFi_LwIP_ControlCommand_t *rhs)
{
  if ((lhs == NULL) || (rhs == NULL))
  {
    return 0U;
  }

  return ((lhs->node == rhs->node) &&
          (lhs->targetTemperature_x10 == rhs->targetTemperature_x10) &&
          (lhs->mode == rhs->mode) &&
          (lhs->fan == rhs->fan) &&
          (lhs->flags == rhs->flags)) ? 1U : 0U;
}

uint8_t APP_WiFi_LwIP_IsNetworkOnline(void)
{
  if ((g_netifAdded == 0U) || (g_netifUp == 0U) || (g_dhcpStarted == 0U))
  {
    return 0U;
  }

  return (dhcp_supplied_address(&g_wifiNetif) != 0) ? 1U : 0U;
}

uint8_t APP_WiFi_LwIP_SendControl(uint8_t node,
                                  int16_t targetTemperature_x10,
                                  uint8_t mode,
                                  uint8_t fan,
                                  uint8_t flags)
{
  APP_WiFi_LwIP_ControlCommand_t command = {0};
  uint8_t roomIndex = 0U;

  if ((APP_WiFi_LwIP_IsKnownNode(node) == 0U) ||
      (APP_HomeData_NodeToRoomIndex(node, &roomIndex) == 0U))
  {
    return 0U;
  }

  command.node = node;
  command.targetTemperature_x10 = targetTemperature_x10;
  command.mode = mode;
  command.fan = fan;
  command.flags = flags;

  if (APP_WiFi_LwIP_EnsureControlMutex() == 0U)
  {
    return 0U;
  }

  if (xSemaphoreTake(g_controlMutex, portMAX_DELAY) != pdTRUE)
  {
    return 0U;
  }

  if ((g_pendingControlValid[roomIndex] != 0U) &&
      (APP_WiFi_LwIP_IsSameControlPayload(&command, &g_pendingControls[roomIndex]) != 0U))
  {
    (void)xSemaphoreGive(g_controlMutex);
    return 1U;
  }

  if ((g_inflightControlValid[roomIndex] != 0U) &&
      (APP_WiFi_LwIP_IsSameControlPayload(&command, &g_inflightControls[roomIndex]) != 0U))
  {
    (void)xSemaphoreGive(g_controlMutex);
    return 1U;
  }

  command.sequence = g_controlSequence++;
  if (g_controlSequence == 0U)
  {
    g_controlSequence = 1U;
  }

  (void)xSemaphoreGive(g_controlMutex);

  if (APP_WiFi_LwIP_StorePendingControl(&command) == 0U)
  {
    return 0U;
  }

  g_serverControlQueuedCount++;
  printf("[proto] control queued #%lu node=%u seq=%u target_x10=%d mode=%u fan=%u flags=0x%02X settle=%ums\n",
         (unsigned long)g_serverControlQueuedCount,
         (unsigned int)command.node,
         (unsigned int)command.sequence,
         (int)command.targetTemperature_x10,
         (unsigned int)command.mode,
         (unsigned int)command.fan,
         (unsigned int)command.flags,
         (unsigned int)APP_WIFI_LWIP_CONTROL_DEBOUNCE_MS);

  return 1U;
}

static void APP_WiFi_LwIP_DropQueuedTx(void)
{
  APP_WiFi_LwIP_TxPacket_t packet = {0};

  for (;;)
  {
    if (APP_WiFi_LwIP_QueuePeek(&packet) == 0U)
    {
      break;
    }

    APP_WiFi_LwIP_QueuePop();
    if (packet.data != NULL)
    {
      vPortFree(packet.data);
    }
  }
}

static void APP_WiFi_LwIP_FlushTxQueue(void)
{
  APP_WiFi_LwIP_TxPacket_t packet = {0};
  uint8_t sentThisCall = 0U;

  if (g_txFlushMutex == NULL)
  {
    return;
  }

  if (xSemaphoreTake(g_txFlushMutex,
                     pdMS_TO_TICKS(APP_WIFI_LWIP_TX_FLUSH_LOCK_WAIT_MS)) != pdTRUE)
  {
    return;
  }

  while (sentThisCall < APP_WIFI_LWIP_TX_BURST_LIMIT)
  {
    APP_WiFiTxStatus_t status = APP_WIFI_TX_STATUS_ERROR;

    if (APP_WiFi_LwIP_QueuePeek(&packet) == 0U)
    {
      break;
    }

    status = APP_WiFi_SendDataFrame(packet.data, packet.length);
    if (status == APP_WIFI_TX_STATUS_OK)
    {
      g_txOkCount++;
      if ((APP_WIFI_LWIP_ETH_TRACE_ENABLE != 0U) && (g_txOkCount <= 32U))
      {
        printf("[lwip] tx ok #%lu len=%u q=%u\n",
               (unsigned long)g_txOkCount,
               (unsigned int)packet.length,
               (unsigned int)g_txQueueCount);
      }

      APP_WiFi_LwIP_QueuePop();
      if (packet.data != NULL)
      {
        vPortFree(packet.data);
      }
      sentThisCall++;
      continue;
    }

    if (status == APP_WIFI_TX_STATUS_ERROR)
    {
      g_txErrorCount++;
      printf("[lwip] dropping queued frame len=%u after tx error\n", (unsigned int)packet.length);
      APP_WiFi_LwIP_QueuePop();
      if (packet.data != NULL)
      {
        vPortFree(packet.data);
      }
    }
    else
    {
      g_txBusyCount++;
      if ((APP_WIFI_LWIP_ETH_TRACE_ENABLE != 0U) && (g_txBusyCount <= 32U))
      {
        printf("[lwip] tx busy #%lu len=%u q=%u\n",
               (unsigned long)g_txBusyCount,
               (unsigned int)packet.length,
               (unsigned int)g_txQueueCount);
      }
    }

    break;
  }

  (void)xSemaphoreGive(g_txFlushMutex);
}

static void APP_WiFi_LwIP_LogRxFrame(const uint8_t *frame, uint16_t length)
{
#if (APP_WIFI_LWIP_ETH_TRACE_ENABLE != 0U)
  uint16_t etherType = 0U;

  if ((frame == NULL) || (length < 14U) || (g_netifRxLogCount >= 48U))
  {
    return;
  }

  g_netifRxLogCount++;
  etherType = (uint16_t)(((uint16_t)frame[12] << 8) | frame[13]);
  printf("[lwip] rx eth #%lu len=%u type=0x%04X src=%02X:%02X:%02X:%02X:%02X:%02X dst=%02X:%02X:%02X:%02X:%02X:%02X\n",
         (unsigned long)g_netifRxLogCount,
         (unsigned int)length,
         (unsigned int)etherType,
         (unsigned int)frame[6],
         (unsigned int)frame[7],
         (unsigned int)frame[8],
         (unsigned int)frame[9],
         (unsigned int)frame[10],
         (unsigned int)frame[11],
         (unsigned int)frame[0],
         (unsigned int)frame[1],
         (unsigned int)frame[2],
         (unsigned int)frame[3],
         (unsigned int)frame[4],
         (unsigned int)frame[5]);

  if ((etherType == 0x0806U) && (length >= 42U))
  {
    const uint16_t arpOp = (uint16_t)(((uint16_t)frame[20] << 8) | frame[21]);
    printf("[lwip] rx arp op=%u spa=%u.%u.%u.%u tpa=%u.%u.%u.%u\n",
           (unsigned int)arpOp,
           (unsigned int)frame[28],
           (unsigned int)frame[29],
           (unsigned int)frame[30],
           (unsigned int)frame[31],
           (unsigned int)frame[38],
           (unsigned int)frame[39],
           (unsigned int)frame[40],
           (unsigned int)frame[41]);
  }
  else if ((etherType == 0x0800U) && (length >= 34U))
  {
    const uint8_t protocol = frame[23];
    const uint16_t ipHeaderLength = (uint16_t)((frame[14] & 0x0FU) * 4U);
    printf("[lwip] rx ip proto=%u src=%u.%u.%u.%u dst=%u.%u.%u.%u\n",
           (unsigned int)protocol,
           (unsigned int)frame[26],
           (unsigned int)frame[27],
           (unsigned int)frame[28],
           (unsigned int)frame[29],
           (unsigned int)frame[30],
           (unsigned int)frame[31],
           (unsigned int)frame[32],
           (unsigned int)frame[33]);

    if ((protocol == 6U) && (ipHeaderLength >= 20U) && (length >= (uint16_t)(14U + ipHeaderLength + 14U)))
    {
      const uint16_t tcpOffset = (uint16_t)(14U + ipHeaderLength);
      const uint16_t srcPort = (uint16_t)(((uint16_t)frame[tcpOffset] << 8) | frame[tcpOffset + 1U]);
      const uint16_t dstPort = (uint16_t)(((uint16_t)frame[tcpOffset + 2U] << 8) | frame[tcpOffset + 3U]);
      printf("[lwip] rx tcp srcPort=%u dstPort=%u flags=0x%02X\n",
             (unsigned int)srcPort,
             (unsigned int)dstPort,
             (unsigned int)frame[tcpOffset + 13U]);
    }
  }
#else
  (void)frame;
  (void)length;
#endif
}

static void APP_WiFi_LwIP_LogTxFrame(const uint8_t *frame, uint16_t length, uint32_t packetIndex)
{
#if (APP_WIFI_LWIP_ETH_TRACE_ENABLE != 0U)
  uint16_t etherType = 0U;

  if ((frame == NULL) || (length < 14U))
  {
    return;
  }

  etherType = (uint16_t)(((uint16_t)frame[12] << 8) | frame[13]);
  printf("[lwip] tx eth #%lu len=%u type=0x%04X src=%02X:%02X:%02X:%02X:%02X:%02X dst=%02X:%02X:%02X:%02X:%02X:%02X\n",
         (unsigned long)packetIndex,
         (unsigned int)length,
         (unsigned int)etherType,
         (unsigned int)frame[6],
         (unsigned int)frame[7],
         (unsigned int)frame[8],
         (unsigned int)frame[9],
         (unsigned int)frame[10],
         (unsigned int)frame[11],
         (unsigned int)frame[0],
         (unsigned int)frame[1],
         (unsigned int)frame[2],
         (unsigned int)frame[3],
         (unsigned int)frame[4],
         (unsigned int)frame[5]);

  if ((etherType == 0x0806U) && (length >= 42U))
  {
    const uint16_t arpOp = (uint16_t)(((uint16_t)frame[20] << 8) | frame[21]);
    printf("[lwip] tx arp op=%u spa=%u.%u.%u.%u tpa=%u.%u.%u.%u\n",
           (unsigned int)arpOp,
           (unsigned int)frame[28],
           (unsigned int)frame[29],
           (unsigned int)frame[30],
           (unsigned int)frame[31],
           (unsigned int)frame[38],
           (unsigned int)frame[39],
           (unsigned int)frame[40],
           (unsigned int)frame[41]);
  }
  else if ((etherType == 0x0800U) && (length >= 54U))
  {
    const uint8_t protocol = frame[23];
    const uint16_t ipHeaderLength = (uint16_t)((frame[14] & 0x0FU) * 4U);
    printf("[lwip] tx ip proto=%u src=%u.%u.%u.%u dst=%u.%u.%u.%u\n",
           (unsigned int)protocol,
           (unsigned int)frame[26],
           (unsigned int)frame[27],
           (unsigned int)frame[28],
           (unsigned int)frame[29],
           (unsigned int)frame[30],
           (unsigned int)frame[31],
           (unsigned int)frame[32],
           (unsigned int)frame[33]);

    if ((protocol == 6U) && (ipHeaderLength >= 20U) && (length >= (uint16_t)(14U + ipHeaderLength + 20U)))
    {
      const uint16_t tcpOffset = (uint16_t)(14U + ipHeaderLength);
      const uint16_t srcPort = (uint16_t)(((uint16_t)frame[tcpOffset] << 8) | frame[tcpOffset + 1U]);
      const uint16_t dstPort = (uint16_t)(((uint16_t)frame[tcpOffset + 2U] << 8) | frame[tcpOffset + 3U]);
      const uint16_t window = (uint16_t)(((uint16_t)frame[tcpOffset + 14U] << 8) | frame[tcpOffset + 15U]);
      const uint16_t checksum = (uint16_t)(((uint16_t)frame[tcpOffset + 16U] << 8) | frame[tcpOffset + 17U]);
      printf("[lwip] tx tcp srcPort=%u dstPort=%u flags=0x%02X win=%u chk=0x%04X\n",
             (unsigned int)srcPort,
             (unsigned int)dstPort,
             (unsigned int)frame[tcpOffset + 13U],
             (unsigned int)window,
             (unsigned int)checksum);
    }
  }
#else
  (void)frame;
  (void)length;
  (void)packetIndex;
#endif
}

static void APP_WiFi_LwIP_LogBufferHex(const char *prefix, const uint8_t *data, uint16_t length)
{
  uint16_t limit = length;
  char line[192];
  int written = 0;

  if ((prefix == NULL) || (data == NULL))
  {
    return;
  }

  if (limit > 32U)
  {
    limit = 32U;
  }

  written = snprintf(line, sizeof(line), "%s len=%u:", prefix, (unsigned int)length);
  if ((written <= 0) || ((size_t)written >= sizeof(line)))
  {
    return;
  }

  for (uint16_t index = 0U; index < limit; index++)
  {
    int chunk = snprintf(&line[written], sizeof(line) - (size_t)written, " %02X", (unsigned int)data[index]);
    if ((chunk <= 0) || ((size_t)chunk >= (sizeof(line) - (size_t)written)))
    {
      break;
    }
    written += chunk;
  }

  if (limit < length)
  {
    if ((size_t)written < (sizeof(line) - 5U))
    {
      (void)snprintf(&line[written], sizeof(line) - (size_t)written, " ...");
    }
  }

  printf("%s\n", line);
}

static uint8_t APP_WiFi_LwIP_SendAll(int socketHandle, const uint8_t *data, uint16_t length)
{
  uint16_t offset = 0U;

  if ((socketHandle < 0) || (data == NULL) || (length == 0U))
  {
    return 0U;
  }

  while (offset < length)
  {
    int sent = send(socketHandle,
                    &data[offset],
                    (size_t)(length - offset),
                    0);
    if (sent <= 0)
    {
      g_serverSendErrorCount++;
      printf("[proto] send failed #%lu after=%u len=%u\n",
             (unsigned long)g_serverSendErrorCount,
             (unsigned int)offset,
             (unsigned int)length);
      return 0U;
    }

    offset = (uint16_t)(offset + (uint16_t)sent);
  }

  return 1U;
}

static uint8_t APP_WiFi_LwIP_SendControlFrame(int clientSocket, const APP_WiFi_LwIP_ControlCommand_t *command)
{
  uint8_t payload[APP_HOME_CONTROL_PAYLOAD_LEN] = {0U};
  uint8_t txFrame[APP_HOME_PROTOCOL_MAX_FRAME_LEN] = {0U};
  uint16_t txLength = 0U;

  if (command == NULL)
  {
    return 0U;
  }

  payload[0] = (uint8_t)(command->targetTemperature_x10 & 0xFF);
  payload[1] = (uint8_t)(((uint16_t)command->targetTemperature_x10 >> 8) & 0xFFU);
  payload[2] = command->mode;
  payload[3] = command->fan;
  payload[4] = command->flags;

  txLength = APP_HomeProtocol_BuildFrame(command->node,
                                         APP_HOME_CMD_CONTROL,
                                         command->sequence,
                                         payload,
                                         (uint16_t)sizeof(payload),
                                         txFrame,
                                         (uint16_t)sizeof(txFrame));
  if ((txLength == 0U) || (APP_WiFi_LwIP_SendAll(clientSocket, txFrame, txLength) == 0U))
  {
    return 0U;
  }

  APP_WiFi_LwIP_FlushTxQueue();

  g_serverControlTxCount++;
  printf("[proto] control tx #%lu node=%u seq=%u target_x10=%d mode=%u fan=%u flags=0x%02X\n",
         (unsigned long)g_serverControlTxCount,
         (unsigned int)command->node,
         (unsigned int)command->sequence,
         (int)command->targetTemperature_x10,
         (unsigned int)command->mode,
         (unsigned int)command->fan,
         (unsigned int)command->flags);

  return 1U;
}

static void APP_WiFi_LwIP_DrainPendingControls(int clientSocket, uint8_t clientNode)
{
  APP_WiFi_LwIP_ControlCommand_t command = {0};
  uint8_t roomIndex = 0U;
  uint8_t hasCommand = 0U;
  TickType_t nowTick = xTaskGetTickCount();

  if ((APP_HomeData_NodeToRoomIndex(clientNode, &roomIndex) == 0U) ||
      (APP_WiFi_LwIP_EnsureControlMutex() == 0U))
  {
    return;
  }

  if (xSemaphoreTake(g_controlMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  if (g_inflightControlValid[roomIndex] != 0U)
  {
    if ((int32_t)(nowTick - g_inflightControlTxTick[roomIndex]) >= (int32_t)pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_ACK_TIMEOUT_MS))
    {
      g_serverControlTimeoutCount++;
      printf("[proto] control ack timeout #%lu node=%u seq=%u\n",
             (unsigned long)g_serverControlTimeoutCount,
             (unsigned int)g_inflightControls[roomIndex].node,
             (unsigned int)g_inflightControls[roomIndex].sequence);
      memset(&g_inflightControls[roomIndex], 0, sizeof(g_inflightControls[roomIndex]));
      g_inflightControlValid[roomIndex] = 0U;
      g_nextControlTxTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_TX_GAP_MS);
    }

    (void)xSemaphoreGive(g_controlMutex);
    return;
  }

  if ((g_nextControlTxTick[roomIndex] != 0U) &&
      ((int32_t)(nowTick - g_nextControlTxTick[roomIndex]) < 0))
  {
    (void)xSemaphoreGive(g_controlMutex);
    return;
  }

  if ((g_pendingControlValid[roomIndex] != 0U) &&
      ((int32_t)(nowTick - g_pendingControlReadyTick[roomIndex]) >= 0))
  {
    command = g_pendingControls[roomIndex];
    g_pendingControlValid[roomIndex] = 0U;
    g_pendingControlReadyTick[roomIndex] = 0U;
    hasCommand = 1U;
  }

  (void)xSemaphoreGive(g_controlMutex);

  if (hasCommand == 0U)
  {
    return;
  }

  if (APP_WiFi_LwIP_SendControlFrame(clientSocket, &command) != 0U)
  {
    if (xSemaphoreTake(g_controlMutex, portMAX_DELAY) == pdTRUE)
    {
      g_inflightControls[roomIndex] = command;
      g_inflightControlValid[roomIndex] = 1U;
      g_inflightControlTxTick[roomIndex] = nowTick;
      (void)xSemaphoreGive(g_controlMutex);
    }
  }
  else
  {
    if (xSemaphoreTake(g_controlMutex, portMAX_DELAY) == pdTRUE)
    {
      g_pendingControls[roomIndex] = command;
      g_pendingControlValid[roomIndex] = 1U;
      g_pendingControlReadyTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_TX_GAP_MS);
      g_nextControlTxTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_TX_GAP_MS);
      (void)xSemaphoreGive(g_controlMutex);
    }
  }
}

static void APP_WiFi_LwIP_HandleControlReply(const APP_HomeProtocolFrame_t *frame, uint8_t isError)
{
  uint8_t roomIndex = 0U;
  TickType_t nowTick = xTaskGetTickCount();

  if ((frame == NULL) ||
      (APP_HomeData_NodeToRoomIndex(frame->node, &roomIndex) == 0U) ||
      (APP_WiFi_LwIP_EnsureControlMutex() == 0U))
  {
    return;
  }

  if (xSemaphoreTake(g_controlMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  if ((g_inflightControlValid[roomIndex] == 0U) ||
      (frame->sequence != g_inflightControls[roomIndex].sequence))
  {
    (void)xSemaphoreGive(g_controlMutex);
    return;
  }

  if ((frame->command == APP_HOME_CMD_ACK) &&
      (frame->length >= 1U) &&
      (frame->payload[0] != APP_HOME_CMD_CONTROL))
  {
    (void)xSemaphoreGive(g_controlMutex);
    return;
  }

  if (isError != 0U)
  {
    g_serverControlErrCount++;
    printf("[proto] control err #%lu node=%u seq=%u\n",
           (unsigned long)g_serverControlErrCount,
           (unsigned int)frame->node,
           (unsigned int)frame->sequence);
  }
  else
  {
    g_serverControlAckCount++;
    printf("[proto] control ack #%lu node=%u seq=%u\n",
           (unsigned long)g_serverControlAckCount,
           (unsigned int)frame->node,
           (unsigned int)frame->sequence);
  }

  memset(&g_inflightControls[roomIndex], 0, sizeof(g_inflightControls[roomIndex]));
  g_inflightControlValid[roomIndex] = 0U;
  g_nextControlTxTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_TX_GAP_MS);
  (void)xSemaphoreGive(g_controlMutex);
}

static void APP_WiFi_LwIP_RequeueInflightControl(uint8_t clientNode)
{
  uint8_t roomIndex = 0U;
  TickType_t nowTick = xTaskGetTickCount();

  if ((APP_HomeData_NodeToRoomIndex(clientNode, &roomIndex) == 0U) ||
      (APP_WiFi_LwIP_EnsureControlMutex() == 0U))
  {
    return;
  }

  if (xSemaphoreTake(g_controlMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  if (g_inflightControlValid[roomIndex] != 0U)
  {
    g_pendingControls[roomIndex] = g_inflightControls[roomIndex];
    g_pendingControlValid[roomIndex] = 1U;
    g_pendingControlReadyTick[roomIndex] = nowTick;
    memset(&g_inflightControls[roomIndex], 0, sizeof(g_inflightControls[roomIndex]));
    g_inflightControlValid[roomIndex] = 0U;
    g_nextControlTxTick[roomIndex] = 0U;
  }
  else if (g_pendingControlValid[roomIndex] != 0U)
  {
    /* Client reconnected path: release pending control immediately. */
    g_pendingControlReadyTick[roomIndex] = nowTick;
    g_nextControlTxTick[roomIndex] = 0U;
  }

  (void)xSemaphoreGive(g_controlMutex);
}

static void APP_WiFi_LwIP_SendProtocolAck(int clientSocket, const APP_HomeProtocolFrame_t *frame)
{
  uint8_t payload[1] = {0U};
  uint8_t txFrame[APP_HOME_PROTOCOL_MAX_FRAME_LEN] = {0U};
  uint16_t txLength = 0U;

  if (frame == NULL)
  {
    return;
  }

  payload[0] = frame->command;
  txLength = APP_HomeProtocol_BuildFrame(frame->node,
                                         APP_HOME_CMD_ACK,
                                         frame->sequence,
                                         payload,
                                         (uint16_t)sizeof(payload),
                                         txFrame,
                                         (uint16_t)sizeof(txFrame));
  if ((txLength != 0U) && (APP_WiFi_LwIP_SendAll(clientSocket, txFrame, txLength) != 0U))
  {
    g_serverAckCount++;
    printf("[proto] ack #%lu node=%u seq=%u cmd=%s\n",
           (unsigned long)g_serverAckCount,
           (unsigned int)frame->node,
           (unsigned int)frame->sequence,
           APP_HomeProtocol_CommandToString(frame->command));
  }
}

static void APP_WiFi_LwIP_SendProtocolError(int clientSocket,
                                            uint8_t node,
                                            uint16_t sequence,
                                            uint8_t command,
                                            APP_HomeProtocolError_t error)
{
  uint8_t payload[2] = {0U};
  uint8_t txFrame[APP_HOME_PROTOCOL_MAX_FRAME_LEN] = {0U};
  uint16_t txLength = 0U;

  payload[0] = command;
  payload[1] = (uint8_t)error;
  txLength = APP_HomeProtocol_BuildFrame(node,
                                         APP_HOME_CMD_ERR,
                                         sequence,
                                         payload,
                                         (uint16_t)sizeof(payload),
                                         txFrame,
                                         (uint16_t)sizeof(txFrame));
  if ((txLength != 0U) && (APP_WiFi_LwIP_SendAll(clientSocket, txFrame, txLength) != 0U))
  {
    g_serverErrCount++;
    printf("[proto] err #%lu node=%u seq=%u cmd=0x%02X err=%u\n",
           (unsigned long)g_serverErrCount,
           (unsigned int)node,
           (unsigned int)sequence,
           (unsigned int)command,
           (unsigned int)error);
  }
}

static uint8_t APP_WiFi_LwIP_IsKnownProtocolCommand(uint8_t command)
{
  switch (command)
  {
    case APP_HOME_CMD_HELLO:
    case APP_HOME_CMD_TELEMETRY:
    case APP_HOME_CMD_CONTROL:
    case APP_HOME_CMD_STATUS:
    case APP_HOME_CMD_HEARTBEAT:
    case APP_HOME_CMD_ACK:
    case APP_HOME_CMD_ERR:
      return 1U;
    default:
      return 0U;
  }
}

static uint8_t APP_WiFi_LwIP_IsKnownNode(uint8_t node)
{
  uint8_t roomIndex = 0U;
  return APP_HomeData_NodeToRoomIndex(node, &roomIndex);
}

static uint16_t APP_WiFi_LwIP_ReadLe16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static int16_t APP_WiFi_LwIP_ReadI16(const uint8_t *data)
{
  return (int16_t)APP_WiFi_LwIP_ReadLe16(data);
}

static void APP_WiFi_LwIP_HandleProtocolFrame(int clientSocket,
                                              const APP_HomeProtocolFrame_t *frame,
                                              uint8_t *clientNode)
{
  uint8_t shouldAck = 1U;
  APP_HomeProtocolError_t error = APP_HOME_ERR_NONE;

  if (frame == NULL)
  {
    return;
  }

  g_serverFrameCount++;
#if APP_WIFI_LWIP_VERBOSE_PROTO_LOG
  printf("[proto] frame #%lu node=%u cmd=%s(0x%02X) seq=%u len=%u\n",
         (unsigned long)g_serverFrameCount,
         (unsigned int)frame->node,
         APP_HomeProtocol_CommandToString(frame->command),
         (unsigned int)frame->command,
         (unsigned int)frame->sequence,
         (unsigned int)frame->length);
#endif

  if ((g_peerProtocolSeen == 0U) &&
      (APP_WiFi_LwIP_IsKnownNode(frame->node) != 0U))
  {
    g_peerProtocolSeen = 1U;
    printf("[lwip] peer protocol seen, stop gratuitous arp\n");
  }

  if (APP_WiFi_LwIP_IsKnownNode(frame->node) != 0U)
  {
    if (clientNode != NULL)
    {
      *clientNode = frame->node;
    }

    (void)APP_HomeData_UpdateNodeOnline(frame->node, 1U);
  }

  if (APP_WiFi_LwIP_IsKnownProtocolCommand(frame->command) == 0U)
  {
    APP_WiFi_LwIP_SendProtocolError(clientSocket,
                                    frame->node,
                                    frame->sequence,
                                    frame->command,
                                    APP_HOME_ERR_BAD_COMMAND);
    return;
  }

  switch (frame->command)
  {
    case APP_HOME_CMD_HELLO:
      if (APP_WiFi_LwIP_IsKnownNode(frame->node) == 0U)
      {
        error = APP_HOME_ERR_BAD_NODE;
      }
      else if (frame->length > 32U)
      {
        error = APP_HOME_ERR_BAD_LENGTH;
      }
      else
      {
        printf("[proto] hello node=%u nameLen=%u\n",
               (unsigned int)frame->node,
               (unsigned int)frame->length);
      }
      break;

    case APP_HOME_CMD_TELEMETRY:
      if (APP_WiFi_LwIP_IsKnownNode(frame->node) == 0U)
      {
        error = APP_HOME_ERR_BAD_NODE;
      }
      else if (frame->length != 8U)
      {
        error = APP_HOME_ERR_BAD_LENGTH;
      }
      else
      {
        const int16_t temperature = APP_WiFi_LwIP_ReadI16(&frame->payload[0]);
        const uint16_t humidity = APP_WiFi_LwIP_ReadLe16(&frame->payload[2]);
        (void)APP_HomeData_UpdateTelemetry(frame->node,
                                           frame->sequence,
                                           temperature,
                                           humidity,
                                           frame->payload[4],
                                           frame->payload[5],
                                           frame->payload[6],
                                           frame->payload[7]);
#if APP_WIFI_LWIP_VERBOSE_PROTO_LOG
        printf("[proto] telemetry node=%u temp_x10=%d hum_x10=%u mode=%u fan=%u online=%u flags=0x%02X\n",
               (unsigned int)frame->node,
               (int)temperature,
               (unsigned int)humidity,
               (unsigned int)frame->payload[4],
               (unsigned int)frame->payload[5],
               (unsigned int)frame->payload[6],
               (unsigned int)frame->payload[7]);
#endif
        shouldAck = 0U;
      }
      break;

    case APP_HOME_CMD_CONTROL:
      if (APP_WiFi_LwIP_IsKnownNode(frame->node) == 0U)
      {
        error = APP_HOME_ERR_BAD_NODE;
      }
      else if (frame->length != APP_HOME_CONTROL_PAYLOAD_LEN)
      {
        error = APP_HOME_ERR_BAD_LENGTH;
      }
      else
      {
        const int16_t target = APP_WiFi_LwIP_ReadI16(&frame->payload[0]);
        printf("[proto] control node=%u target_x10=%d mode=%u fan=%u flags=0x%02X\n",
               (unsigned int)frame->node,
               (int)target,
               (unsigned int)frame->payload[2],
               (unsigned int)frame->payload[3],
               (unsigned int)frame->payload[4]);
      }
      break;

    case APP_HOME_CMD_STATUS:
      if (APP_WiFi_LwIP_IsKnownNode(frame->node) == 0U)
      {
        error = APP_HOME_ERR_BAD_NODE;
      }
      else if (frame->length < 1U)
      {
        error = APP_HOME_ERR_BAD_LENGTH;
      }
      else
      {
#if APP_WIFI_LWIP_VERBOSE_PROTO_LOG
        printf("[proto] status node=%u code=%u\n",
               (unsigned int)frame->node,
               (unsigned int)frame->payload[0]);
#endif
      }
      break;

    case APP_HOME_CMD_HEARTBEAT:
      if (APP_WiFi_LwIP_IsKnownNode(frame->node) == 0U)
      {
        error = APP_HOME_ERR_BAD_NODE;
      }
      else if (frame->length != 0U)
      {
        error = APP_HOME_ERR_BAD_LENGTH;
      }
      else
      {
#if APP_WIFI_LWIP_VERBOSE_PROTO_LOG
        printf("[proto] heartbeat node=%u\n", (unsigned int)frame->node);
#endif
      }
      break;

    case APP_HOME_CMD_ACK:
      shouldAck = 0U;
      printf("[proto] peer ack node=%u seq=%u\n",
             (unsigned int)frame->node,
             (unsigned int)frame->sequence);
      APP_WiFi_LwIP_HandleControlReply(frame, 0U);
      break;

    case APP_HOME_CMD_ERR:
      shouldAck = 0U;
      printf("[proto] peer err node=%u seq=%u\n",
             (unsigned int)frame->node,
             (unsigned int)frame->sequence);
      APP_WiFi_LwIP_HandleControlReply(frame, 1U);
      break;

    default:
      error = APP_HOME_ERR_BAD_COMMAND;
      break;
  }

  if (error != APP_HOME_ERR_NONE)
  {
    APP_WiFi_LwIP_SendProtocolError(clientSocket,
                                    frame->node,
                                    frame->sequence,
                                    frame->command,
                                    error);
    return;
  }

  if (shouldAck != 0U)
  {
    APP_WiFi_LwIP_SendProtocolAck(clientSocket, frame);
  }
}

static void APP_WiFi_LwIP_StartServerTask(void)
{
  if (g_serverStarted != 0U)
  {
    return;
  }

  if (sys_thread_new("wifi_tcp", APP_WiFi_LwIP_ServerTask, NULL, APP_WIFI_LWIP_SOCKET_STACK_BYTES, (int)APP_WIFI_LWIP_SOCKET_PRIORITY) == NULL)
  {
    printf("[lwip] failed to start TCP server thread\n");
    return;
  }

  g_serverStarted = 1U;
}

static void APP_WiFi_LwIP_ClientTask(void *argument)
{
  APP_WiFi_LwIP_ClientContext_t *context = (APP_WiFi_LwIP_ClientContext_t *)argument;
  APP_HomeProtocolParser_t parser;
  APP_HomeProtocolFrame_t frame;
  uint8_t clientNode = APP_HOME_NODE_NONE;
  int clientSocket = -1;

  if (context == NULL)
  {
    vTaskDelete(NULL);
    return;
  }

  clientSocket = context->socketHandle;
  vPortFree(context);

  APP_HomeProtocol_InitParser(&parser);
  memset(&frame, 0, sizeof(frame));

  for (;;)
  {
    uint8_t buffer[256];
    int received = 0;

    APP_WiFi_LwIP_DrainPendingControls(clientSocket, clientNode);

    received = recv(clientSocket, buffer, (int)sizeof(buffer), MSG_DONTWAIT);
    if (received < 0)
    {
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
      {
        vTaskDelay(pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_POLL_MS));
        continue;
      }

      printf("[lwip] recv failed errno=%d\n", errno);
      break;
    }

    if (received == 0)
    {
      printf("[lwip] peer closed connection\n");
      break;
    }

    g_serverRxCount++;
    if (g_serverRxCount <= APP_WIFI_LWIP_RX_LOG_LIMIT)
    {
      printf("[lwip] recv #%lu len=%d\n",
             (unsigned long)g_serverRxCount,
             received);
      APP_WiFi_LwIP_LogBufferHex("[lwip] recv hex", buffer, (uint16_t)received);
    }

    for (int index = 0; index < received; index++)
    {
      APP_HomeProtocolParseResult_t parseResult =
        APP_HomeProtocol_PushByte(&parser, buffer[index], &frame);

      if (parseResult == APP_HOME_PARSE_FRAME)
      {
        APP_WiFi_LwIP_HandleProtocolFrame(clientSocket, &frame, &clientNode);
      }
      else if (parseResult == APP_HOME_PARSE_ERROR)
      {
        printf("[proto] parse error=%u\n", (unsigned int)parser.lastError);
        APP_WiFi_LwIP_SendProtocolError(clientSocket,
                                        APP_HOME_NODE_NONE,
                                        0U,
                                        0U,
                                        parser.lastError);
      }
    }
  }

  if (APP_WiFi_LwIP_IsKnownNode(clientNode) != 0U)
  {
    (void)APP_HomeData_UpdateNodeOnline(clientNode, 0U);
  }
  APP_WiFi_LwIP_RequeueInflightControl(clientNode);

  closesocket(clientSocket);
  printf("[lwip] client disconnected node=%u\n", (unsigned int)clientNode);
  vTaskDelete(NULL);
}

static void APP_WiFi_LwIP_ServerTask(void *argument)
{
  (void)argument;

  for (;;)
  {
    while (APP_WiFi_LwIP_IsNetworkOnline() == 0U)
    {
      vTaskDelay(pdMS_TO_TICKS(200U));
    }

    int listenSocket = -1;
    struct sockaddr_in serverAddress;
    int reuse = 1;
    int status = 0;
    unsigned long nonBlocking = 1UL;

    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0)
    {
      printf("[lwip] socket create failed\n");
      vTaskDelay(pdMS_TO_TICKS(1000U));
      continue;
    }

    (void)setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, (socklen_t)sizeof(reuse));

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(APP_WIFI_LWIP_TCP_SERVER_PORT);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    status = bind(listenSocket, (struct sockaddr *)&serverAddress, (socklen_t)sizeof(serverAddress));
    if (status != 0)
    {
      printf("[lwip] bind failed\n");
      closesocket(listenSocket);
      vTaskDelay(pdMS_TO_TICKS(1000U));
      continue;
    }

    status = listen(listenSocket, (int)(APP_HOME_DATA_ROOM_COUNT + 1U));
    if (status != 0)
    {
      printf("[lwip] listen failed\n");
      closesocket(listenSocket);
      vTaskDelay(pdMS_TO_TICKS(1000U));
      continue;
    }

    if (ioctl(listenSocket, FIONBIO, &nonBlocking) != 0)
    {
      printf("[lwip] listen nonblocking failed\n");
      closesocket(listenSocket);
      vTaskDelay(pdMS_TO_TICKS(1000U));
      continue;
    }

    printf("[lwip] tcp server listening on %u\n", (unsigned int)APP_WIFI_LWIP_TCP_SERVER_PORT);

    for (;;)
    {
      struct sockaddr_in clientAddress;
      socklen_t clientLength = (socklen_t)sizeof(clientAddress);
      int clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &clientLength);

      if (clientSocket < 0)
      {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
        {
          if (APP_WiFi_LwIP_IsNetworkOnline() == 0U)
          {
            printf("[lwip] network offline, rebuild server socket\n");
            break;
          }

          vTaskDelay(pdMS_TO_TICKS(50U));
          continue;
        }

        printf("[lwip] accept failed errno=%d\n", errno);
        vTaskDelay(pdMS_TO_TICKS(200U));
        continue;
      }

      {
        const uint32_t clientIp = ntohl(clientAddress.sin_addr.s_addr);
        APP_WiFi_LwIP_ClientContext_t *clientContext = NULL;
        int noDelay = 1;
        int keepAlive = 1;

        (void)setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &noDelay, (socklen_t)sizeof(noDelay));
        (void)setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, (socklen_t)sizeof(keepAlive));
        printf("[lwip] client connected %lu.%lu.%lu.%lu:%u\n",
               (unsigned long)((clientIp >> 24) & 0xFFUL),
               (unsigned long)((clientIp >> 16) & 0xFFUL),
               (unsigned long)((clientIp >> 8) & 0xFFUL),
               (unsigned long)(clientIp & 0xFFUL),
               (unsigned int)ntohs(clientAddress.sin_port));

        clientContext = (APP_WiFi_LwIP_ClientContext_t *)pvPortMalloc(sizeof(APP_WiFi_LwIP_ClientContext_t));
        if (clientContext == NULL)
        {
          printf("[lwip] client context alloc failed\n");
          closesocket(clientSocket);
          continue;
        }

        clientContext->socketHandle = clientSocket;
        if (sys_thread_new("wifi_cli",
                           APP_WiFi_LwIP_ClientTask,
                           clientContext,
                           APP_WIFI_LWIP_CLIENT_STACK_BYTES,
                           (int)APP_WIFI_LWIP_SOCKET_PRIORITY) == NULL)
        {
          printf("[lwip] failed to start client thread\n");
          vPortFree(clientContext);
          closesocket(clientSocket);
          continue;
        }
      }
    }

    closesocket(listenSocket);
    vTaskDelay(pdMS_TO_TICKS(1000U));
  }
}

static void APP_WiFi_LwIP_EnsureInitialized(void)
{
  ip4_addr_t ipaddr;
  ip4_addr_t netmask;
  ip4_addr_t gw;

  if (g_tcpipStarted == 0U)
  {
    g_tcpipInitSemaphore = xSemaphoreCreateBinary();
    if (g_tcpipInitSemaphore == NULL)
    {
      printf("[lwip] tcpip init semaphore allocation failed\n");
      return;
    }

    tcpip_init(APP_WiFi_LwIP_TcpipInitDone, &g_tcpipInitSemaphore);
    if (xSemaphoreTake(g_tcpipInitSemaphore, portMAX_DELAY) != pdTRUE)
    {
      printf("[lwip] tcpip init wait failed\n");
      vSemaphoreDelete(g_tcpipInitSemaphore);
      g_tcpipInitSemaphore = NULL;
      return;
    }

    vSemaphoreDelete(g_tcpipInitSemaphore);
    g_tcpipInitSemaphore = NULL;
    g_tcpipStarted = 1U;
  }

  if (g_txQueueMutex == NULL)
  {
    g_txQueueMutex = xSemaphoreCreateMutex();
    if (g_txQueueMutex == NULL)
    {
      printf("[lwip] tx queue mutex allocation failed\n");
      return;
    }
  }

  if (g_txFlushMutex == NULL)
  {
    g_txFlushMutex = xSemaphoreCreateMutex();
    if (g_txFlushMutex == NULL)
    {
      printf("[lwip] tx flush mutex allocation failed\n");
      return;
    }
  }

  if (APP_WiFi_LwIP_EnsureControlMutex() == 0U)
  {
    printf("[lwip] control mutex allocation failed\n");
    return;
  }

  if (g_netifAdded == 0U)
  {
    err_t addResult = ERR_OK;

    ip4_addr_set_zero(&ipaddr);
    ip4_addr_set_zero(&netmask);
    ip4_addr_set_zero(&gw);

    memset(&g_wifiNetif, 0, sizeof(g_wifiNetif));
    addResult = netifapi_netif_add(&g_wifiNetif,
                                   &ipaddr,
                                   &netmask,
                                   &gw,
                                   NULL,
                                   APP_WiFi_LwIP_NetifInit,
                                   tcpip_input);
    if (addResult != ERR_OK)
    {
      printf("[lwip] netif add failed: %d\n", (int)addResult);
      return;
    }

    (void)netifapi_netif_set_default(&g_wifiNetif);
    (void)netifapi_netif_set_down(&g_wifiNetif);
    g_netifAdded = 1U;
    g_netifMacSynced = 0U;
    printf("[lwip] netif registered\n");
  }

  if (g_serverStarted == 0U)
  {
    APP_WiFi_LwIP_StartServerTask();
  }

  if ((g_lwipReadyLogged == 0U) &&
      (g_tcpipStarted != 0U) &&
      (g_netifAdded != 0U) &&
      (g_serverStarted != 0U))
  {
    printf("[lwip] stack initialized\n");
    g_lwipReadyLogged = 1U;
  }
}

void APP_WiFi_LwIP_ProcessEthernetFrame(const uint8_t *frame, uint16_t length)
{
  struct pbuf *packet = NULL;
  err_t result = ERR_OK;

  if ((frame == NULL) || (length == 0U))
  {
    return;
  }

  APP_WiFi_LwIP_LogRxFrame(frame, length);

  APP_WiFi_LwIP_EnsureInitialized();
  if ((g_netifAdded == 0U) || (g_netifUp == 0U))
  {
    return;
  }

  packet = pbuf_alloc(PBUF_RAW, length, PBUF_POOL_RX);
  if (packet == NULL)
  {
    return;
  }

  result = pbuf_take(packet, frame, length);
  if (result != ERR_OK)
  {
    pbuf_free(packet);
    return;
  }

  result = tcpip_input(packet, &g_wifiNetif);
  if (result != ERR_OK)
  {
    pbuf_free(packet);
    return;
  }

  APP_WiFi_LwIP_FlushTxQueue();
}

void APP_WiFi_LwIP_Service(void)
{
  uint8_t linkConnected = (APP_WiFi_GetLinkState() == APP_WIFI_LINK_STATE_CONNECTED) ? 1U : 0U;

  APP_WiFi_LwIP_EnsureInitialized();
  if (g_netifAdded == 0U)
  {
    return;
  }

  if (linkConnected != 0U)
  {
    if (g_netifMacSynced == 0U)
    {
      if (netifapi_netif_common(&g_wifiNetif, NULL, APP_WiFi_LwIP_RefreshNetifMacAddress) != ERR_OK)
      {
        return;
      }

      g_netifMacSynced = 1U;
    }

    if (g_netifUp == 0U)
    {
      if ((netifapi_netif_set_link_up(&g_wifiNetif) == ERR_OK) &&
          (netifapi_netif_set_up(&g_wifiNetif) == ERR_OK))
      {
        g_netifUp = 1U;
        g_dhcpStarted = 0U;
        g_ipAnnounced = 0U;
        g_peerProtocolSeen = 0U;
        g_dhcpStartTick = 0U;
        g_nextDhcpWaitLogTick = 0U;
        g_dhcpRestartCount = 0U;
        APP_WiFi_LwIP_ResetControlState();
        printf("[lwip] netif up, starting DHCP\n");
      }
      else
      {
        return;
      }
    }

    if (g_dhcpStarted == 0U)
    {
      APP_WiFi_LwIP_StartDhcp();
    }

    APP_WiFi_LwIP_CheckDhcpProgress();

    if ((g_ipAnnounced == 0U) && dhcp_supplied_address(&g_wifiNetif))
    {
      char ipBuffer[16] = {0};

      if (ip4addr_ntoa_r(netif_ip4_addr(&g_wifiNetif), ipBuffer, sizeof(ipBuffer)) != NULL)
      {
        printf("[lwip] dhcp bound ip=%s\n", ipBuffer);
      }
      else
      {
        printf("[lwip] dhcp bound\n");
      }

      g_ipAnnounced = 1U;
      g_nextArpRefreshTick = 0U;
    }

    APP_WiFi_LwIP_FlushTxQueue();
    APP_WiFi_LwIP_MaybeRefreshArp();
    APP_WiFi_LwIP_FlushTxQueue();
  }
  else
  {
    if (g_netifUp != 0U)
    {
      if (g_dhcpStarted != 0U)
      {
        (void)netifapi_dhcp_stop(&g_wifiNetif);
        g_dhcpStarted = 0U;
      }

      (void)netifapi_netif_set_link_down(&g_wifiNetif);
      (void)netifapi_netif_set_down(&g_wifiNetif);
      g_netifUp = 0U;
      g_netifMacSynced = 0U;
      g_ipAnnounced = 0U;
      g_peerProtocolSeen = 0U;
      g_dhcpStartTick = 0U;
      g_nextDhcpWaitLogTick = 0U;
      g_dhcpRestartCount = 0U;
      g_nextArpRefreshTick = 0U;
      g_arpRefreshCount = 0U;
      APP_WiFi_LwIP_ResetControlState();
      APP_WiFi_LwIP_DropQueuedTx();
      printf("[lwip] netif down\n");
    }
  }
}
