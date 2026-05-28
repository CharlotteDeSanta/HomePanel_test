#include "app_wifi_lwip.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

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
#define APP_WIFI_LWIP_TX_PRIORITY_QUEUE_DEPTH 16U
#define APP_WIFI_LWIP_TX_BURST_LIMIT        8U
#define APP_WIFI_LWIP_TX_FLUSH_LOCK_WAIT_MS 2U
#define APP_WIFI_LWIP_TX_ERROR_RETRY_BEFORE_DROP 8U
#define APP_WIFI_LWIP_CONTROL_ROOM_COUNT    APP_HOME_DATA_ROOM_COUNT
#define APP_WIFI_LWIP_CONTROL_POLL_MS       2U
#define APP_WIFI_LWIP_CONTROL_TX_GAP_MS      40U
#define APP_WIFI_LWIP_CONTROL_DEBOUNCE_MS    40U
#define APP_WIFI_LWIP_CONTROL_GLOBAL_TX_GAP_MS 180U
#define APP_WIFI_LWIP_CONTROL_RETRY_MS       APP_WIFI_LWIP_CONTROL_TX_GAP_MS
#define APP_WIFI_LWIP_CONTROL_SUPERSEDE_MS   250U
#define APP_WIFI_LWIP_GRATUITOUS_ARP_ENABLE 0U
#define APP_WIFI_LWIP_INFO_LOG_ENABLE       0U
#define APP_WIFI_LWIP_ARP_REFRESH_MS        5000U
#define APP_WIFI_LWIP_ARP_REFRESH_MAX       6U
#define APP_WIFI_LWIP_DHCP_WAIT_LOG_MS      5000U
#define APP_WIFI_LWIP_DHCP_RESTART_MS       15000U
#define APP_WIFI_LWIP_ETH_TRACE_ENABLE      0U
#define APP_WIFI_LWIP_SOCKET_STACK_BYTES    4096U
#define APP_WIFI_LWIP_CLIENT_STACK_BYTES    4096U
#define APP_WIFI_LWIP_SOCKET_PRIORITY       25U
#define APP_WIFI_LWIP_LISTEN_BACKLOG        8U
#define APP_WIFI_LWIP_ACCEPT_POLL_MS        2U
#define APP_WIFI_LWIP_CLIENT_RECV_TIMEOUT_MS 80U
#define APP_WIFI_LWIP_CLIENT_SEND_TIMEOUT_MS 500U
#define APP_WIFI_LWIP_CLIENT_IDLE_TIMEOUT_MS 0U
#define APP_WIFI_LWIP_CLIENT_RX_YIELD_MS    1U
#define APP_WIFI_LWIP_MAX_CLIENTS            (APP_HOME_DATA_ROOM_COUNT + 4U)
#define APP_WIFI_LWIP_RX_LOG_LIMIT          0U
#define APP_WIFI_LWIP_VERBOSE_PROTO_LOG     0U
#define APP_WIFI_LWIP_CLIENT_RX_BUFFER_SIZE 512U
#define APP_WIFI_LWIP_PEER_CACHE_SIZE       (APP_WIFI_LWIP_MAX_CLIENTS + 2U)
#define APP_WIFI_LWIP_IPV4_STR_LEN          16U
#define APP_WIFI_LWIP_MAC_STR_LEN           18U
#define APP_WIFI_LWIP_SEND_WOULDBLOCK_RETRY_MAX 2U
#define APP_WIFI_PROTO_FAN_AUTO               4U

typedef struct
{
  uint8_t *data;
  uint16_t length;
  uint8_t priority;
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
  uint8_t slotIndex;
  uint32_t ipAddress;
  uint16_t port;
} APP_WiFi_LwIP_ClientContext_t;

typedef struct
{
  int socketHandle;
  uint32_t ipAddress;
  uint8_t node;
  uint8_t active;
  uint8_t closeRequested;
} APP_WiFi_LwIP_ClientSlot_t;

typedef struct
{
  uint32_t ipAddress;
  uint8_t macAddress[APP_WIFI_MAC_ADDRESS_SIZE];
  TickType_t lastSeenTick;
  uint8_t valid;
} APP_WiFi_LwIP_PeerCacheEntry_t;

static struct netif g_wifiNetif = {0};
static SemaphoreHandle_t g_tcpipInitSemaphore = NULL;
static SemaphoreHandle_t g_txQueueMutex = NULL;
static SemaphoreHandle_t g_txFlushMutex = NULL;
static SemaphoreHandle_t g_controlMutex = NULL;
static SemaphoreHandle_t g_clientSlotsMutex = NULL;
static APP_WiFi_LwIP_TxPacket_t g_txQueue[APP_WIFI_LWIP_TX_QUEUE_DEPTH] = {0};
static APP_WiFi_LwIP_TxPacket_t g_txPriorityQueue[APP_WIFI_LWIP_TX_PRIORITY_QUEUE_DEPTH] = {0};
static APP_WiFi_LwIP_ClientSlot_t g_clientSlots[APP_WIFI_LWIP_MAX_CLIENTS] = {0};
static APP_WiFi_LwIP_PeerCacheEntry_t g_peerCache[APP_WIFI_LWIP_PEER_CACHE_SIZE] = {0};
static APP_WiFi_LwIP_ControlCommand_t g_pendingControls[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static uint8_t g_pendingControlValid[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static uint8_t g_pendingControlRetryCount[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static uint8_t g_txQueueHead = 0U;
static uint8_t g_txQueueTail = 0U;
static uint8_t g_txQueueCount = 0U;
static uint8_t g_txPriorityQueueHead = 0U;
static uint8_t g_txPriorityQueueTail = 0U;
static uint8_t g_txPriorityQueueCount = 0U;
static uint16_t g_controlSequence = 1U;
static TickType_t g_pendingControlReadyTick[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static TickType_t g_nextControlTxTick[APP_WIFI_LWIP_CONTROL_ROOM_COUNT] = {0};
static TickType_t g_nextGlobalControlTxTick = 0U;
static uint8_t g_tcpipStarted = 0U;
static uint8_t g_netifAdded = 0U;
static uint8_t g_netifUp = 0U;
static uint8_t g_netifMacSynced = 0U;
static uint8_t g_dhcpStarted = 0U;
static uint8_t g_ipAnnounced = 0U;
static uint8_t g_peerProtocolSeen = 0U;
static uint8_t g_serverStarted = 0U;
static uint8_t g_lwipReadyLogged = 0U;
static volatile uint8_t g_serverRebuildRequested = 0U;
static TickType_t g_nextArpRefreshTick = 0U;
static TickType_t g_dhcpStartTick = 0U;
static TickType_t g_nextDhcpWaitLogTick = 0U;
static uint32_t g_linkOutputCount = 0U;
static uint32_t g_txOkCount = 0U;
static uint32_t g_txBusyCount = 0U;
static uint32_t g_txErrorCount = 0U;
static uint32_t g_txEnqueueFailCount = 0U;
static uint32_t g_txQueueDropOldestCount = 0U;
static uint8_t *g_txLastErrorHeadData = NULL;
static uint16_t g_txLastErrorHeadLen = 0U;
static uint8_t g_txHeadErrorStreak = 0U;
static uint32_t g_arpRefreshCount = 0U;
#if (APP_WIFI_LWIP_ETH_TRACE_ENABLE != 0U)
static uint32_t g_netifRxLogCount = 0U;
#endif
static uint32_t g_serverRxCount = 0U;
static uint32_t g_serverFrameCount = 0U;
static uint32_t g_serverAckCount = 0U;
static uint32_t g_serverErrCount = 0U;
static uint32_t g_serverControlQueuedCount = 0U;
static uint32_t g_serverControlTxCount = 0U;
static uint32_t g_serverSendErrorCount = 0U;
static uint32_t g_dhcpRestartCount = 0U;
static uint32_t g_rxPbufAllocFailCount = 0U;
static uint32_t g_rxTcpipInputFailCount = 0U;
static uint32_t g_rxEthernetFrameCount = 0U;

static void APP_WiFi_LwIP_TcpipInitDone(void *arg);
static err_t APP_WiFi_LwIP_NetifInit(struct netif *netif);
static err_t APP_WiFi_LwIP_LinkOutput(struct netif *netif, struct pbuf *p);
static uint8_t APP_WiFi_LwIP_IsPriorityTxFrame(const uint8_t *data, uint16_t length);
static uint8_t APP_WiFi_LwIP_QueuePush(uint8_t *data, uint16_t length, uint8_t priority);
static uint8_t APP_WiFi_LwIP_QueuePeek(APP_WiFi_LwIP_TxPacket_t *packet);
static void APP_WiFi_LwIP_QueuePop(uint8_t priority);
static void APP_WiFi_LwIP_DropQueuedTx(void);
static void APP_WiFi_LwIP_FlushTxQueue(void);
static void APP_WiFi_LwIP_LogRuntimeStats(const char *source);
static void APP_WiFi_LwIP_LogRxFrame(const uint8_t *frame, uint16_t length);
static void APP_WiFi_LwIP_LogTxFrame(const uint8_t *frame, uint16_t length, uint32_t packetIndex);
static void APP_WiFi_LwIP_LogBufferHex(const char *prefix, const uint8_t *data, uint16_t length);
static void APP_WiFi_LwIP_StartServerTask(void);
static void APP_WiFi_LwIP_ServerTask(void *argument);
static void APP_WiFi_LwIP_ClientTask(void *argument);
static void APP_WiFi_LwIP_EnsureInitialized(void);
static err_t APP_WiFi_LwIP_RefreshNetifMacAddress(struct netif *netif);
#if (APP_WIFI_LWIP_GRATUITOUS_ARP_ENABLE != 0U)
static err_t APP_WiFi_LwIP_SendGratuitousArp(struct netif *netif);
static void APP_WiFi_LwIP_MaybeRefreshArp(void);
#endif
static void APP_WiFi_LwIP_StartDhcp(void);
static void APP_WiFi_LwIP_CheckDhcpProgress(void);
static uint8_t APP_WiFi_LwIP_EnsureControlMutex(void);
static void APP_WiFi_LwIP_ResetControlState(void);
static uint8_t APP_WiFi_LwIP_StorePendingControlEx(const APP_WiFi_LwIP_ControlCommand_t *command,
                                                   uint8_t retryCount);
static uint8_t APP_WiFi_LwIP_StorePendingControl(const APP_WiFi_LwIP_ControlCommand_t *command);
static uint8_t APP_WiFi_LwIP_IsSameControlPayload(const APP_WiFi_LwIP_ControlCommand_t *lhs,
                                                  const APP_WiFi_LwIP_ControlCommand_t *rhs);
static uint8_t APP_WiFi_LwIP_SendAll(int socketHandle, const uint8_t *data, uint16_t length);
static void APP_WiFi_LwIP_AbortSocket(int socketHandle);
static uint8_t APP_WiFi_LwIP_SendControlFrame(int clientSocket, const APP_WiFi_LwIP_ControlCommand_t *command);
static void APP_WiFi_LwIP_DrainPendingControls(int clientSocket, uint8_t clientNode);
static uint8_t APP_WiFi_LwIP_FindActiveNodeSocket(uint8_t node, int *socketHandle);
static void APP_WiFi_LwIP_DrainAllPendingControls(void);
static void APP_WiFi_LwIP_HandleProtocolFrame(int clientSocket,
                                              uint8_t slotIndex,
                                              const APP_HomeProtocolFrame_t *frame,
                                              uint8_t *clientNode);
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
static uint8_t APP_WiFi_LwIP_EnsureClientSlotsMutex(void);
static int32_t APP_WiFi_LwIP_ClaimClientSlot(uint32_t ipAddress, int socketHandle);
static void APP_WiFi_LwIP_RequestNodeTakeover(uint8_t slotIndex, uint8_t node);
static uint8_t APP_WiFi_LwIP_IsCloseRequested(uint8_t slotIndex);
static uint8_t APP_WiFi_LwIP_HasOtherActiveNodeClient(uint8_t slotIndex, uint8_t node);
static uint8_t APP_WiFi_LwIP_GetSlotSnapshot(uint8_t slotIndex, uint32_t *ipAddress, uint8_t *node);
static void APP_WiFi_LwIP_ReleaseClientSlot(uint8_t slotIndex);
static void APP_WiFi_LwIP_RequestCloseAllClients(void);
static uint32_t APP_WiFi_LwIP_BuildIpAddressU32(uint8_t octet0, uint8_t octet1, uint8_t octet2, uint8_t octet3);
static void APP_WiFi_LwIP_FormatIpAddress(uint32_t ipAddress, char *buffer, uint32_t bufferSize);
static void APP_WiFi_LwIP_FormatMacAddress(const uint8_t *macAddress, char *buffer, uint32_t bufferSize);
static void APP_WiFi_LwIP_UpdatePeerCache(uint32_t ipAddress, const uint8_t *macAddress);
static uint8_t APP_WiFi_LwIP_FindPeerMac(uint32_t ipAddress, uint8_t *macAddress, uint32_t *ageMs);
static void APP_WiFi_LwIP_CachePeerFromFrame(const uint8_t *frame, uint16_t length);
static void APP_WiFi_LwIP_LogClientEndpoint(const char *eventName,
                                            uint8_t slotIndex,
                                            uint8_t node,
                                            uint32_t ipAddress,
                                            uint16_t port,
                                            int32_t extraValue,
                                            const char *extraLabel);
static void APP_WiFi_LwIP_RegisterQueueForDebug(QueueHandle_t handle, const char *name);

/* tcpip_init completion callback used to release init wait semaphore. */
static void APP_WiFi_LwIP_TcpipInitDone(void *arg)
{
  SemaphoreHandle_t *doneSemaphore = (SemaphoreHandle_t *)arg;

  if (doneSemaphore != NULL)
  {
    (void)xSemaphoreGive(*doneSemaphore);
  }
}

/* Register RTOS objects so kernel-aware debugger can show readable names. */
static void APP_WiFi_LwIP_RegisterQueueForDebug(QueueHandle_t handle, const char *name)
{
#if (configQUEUE_REGISTRY_SIZE > 0)
  if ((handle != NULL) && (name != NULL))
  {
    vQueueAddToRegistry(handle, (char *)name);
  }
#else
  (void)handle;
  (void)name;
#endif
}

/* Pack IPv4 octets into host-order 32-bit cache key. */
static uint32_t APP_WiFi_LwIP_BuildIpAddressU32(uint8_t octet0, uint8_t octet1, uint8_t octet2, uint8_t octet3)
{
  return ((uint32_t)octet0 << 24) |
         ((uint32_t)octet1 << 16) |
         ((uint32_t)octet2 << 8) |
         (uint32_t)octet3;
}

/* Format IPv4 address cache key into dotted-decimal string. */
static void APP_WiFi_LwIP_FormatIpAddress(uint32_t ipAddress, char *buffer, uint32_t bufferSize)
{
  if ((buffer == NULL) || (bufferSize == 0U))
  {
    return;
  }

  (void)snprintf(buffer,
                 (size_t)bufferSize,
                 "%lu.%lu.%lu.%lu",
                 (unsigned long)((ipAddress >> 24) & 0xFFUL),
                 (unsigned long)((ipAddress >> 16) & 0xFFUL),
                 (unsigned long)((ipAddress >> 8) & 0xFFUL),
                 (unsigned long)(ipAddress & 0xFFUL));
}

/* Format MAC address bytes into colon-separated debug string. */
static void APP_WiFi_LwIP_FormatMacAddress(const uint8_t *macAddress, char *buffer, uint32_t bufferSize)
{
  if ((buffer == NULL) || (bufferSize == 0U))
  {
    return;
  }

  if (macAddress == NULL)
  {
    (void)snprintf(buffer, (size_t)bufferSize, "unknown");
    return;
  }

  (void)snprintf(buffer,
                 (size_t)bufferSize,
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 (unsigned int)macAddress[0],
                 (unsigned int)macAddress[1],
                 (unsigned int)macAddress[2],
                 (unsigned int)macAddress[3],
                 (unsigned int)macAddress[4],
                 (unsigned int)macAddress[5]);
}

/* Update lightweight peer ARP cache from observed traffic. */
static void APP_WiFi_LwIP_UpdatePeerCache(uint32_t ipAddress, const uint8_t *macAddress)
{
  TickType_t nowTick = xTaskGetTickCount();
  int32_t freeIndex = -1;
  int32_t oldestIndex = 0;
  TickType_t oldestTick = 0U;
  uint8_t oldestSet = 0U;

  if ((ipAddress == 0U) || (macAddress == NULL))
  {
    return;
  }

  taskENTER_CRITICAL();
  for (uint32_t index = 0U; index < APP_WIFI_LWIP_PEER_CACHE_SIZE; index++)
  {
    APP_WiFi_LwIP_PeerCacheEntry_t *entry = &g_peerCache[index];

    if ((entry->valid != 0U) && (entry->ipAddress == ipAddress))
    {
      if (memcmp(entry->macAddress, macAddress, APP_WIFI_MAC_ADDRESS_SIZE) != 0)
      {
        memcpy(entry->macAddress, macAddress, APP_WIFI_MAC_ADDRESS_SIZE);
      }
      entry->lastSeenTick = nowTick;
      taskEXIT_CRITICAL();
      return;
    }

    if ((entry->valid == 0U) && (freeIndex < 0))
    {
      freeIndex = (int32_t)index;
    }

    if ((entry->valid != 0U) &&
        ((oldestSet == 0U) || ((int32_t)(entry->lastSeenTick - oldestTick) < 0)))
    {
      oldestTick = entry->lastSeenTick;
      oldestIndex = (int32_t)index;
      oldestSet = 1U;
    }
  }

  if (freeIndex < 0)
  {
    freeIndex = oldestIndex;
  }

  g_peerCache[(uint32_t)freeIndex].ipAddress = ipAddress;
  memcpy(g_peerCache[(uint32_t)freeIndex].macAddress, macAddress, APP_WIFI_MAC_ADDRESS_SIZE);
  g_peerCache[(uint32_t)freeIndex].lastSeenTick = nowTick;
  g_peerCache[(uint32_t)freeIndex].valid = 1U;
  taskEXIT_CRITICAL();
}

/* Resolve peer MAC from local cache for faster diagnostics/heuristics. */
static uint8_t APP_WiFi_LwIP_FindPeerMac(uint32_t ipAddress, uint8_t *macAddress, uint32_t *ageMs)
{
  TickType_t nowTick = xTaskGetTickCount();
  uint8_t found = 0U;

  if ((ipAddress == 0U) || (macAddress == NULL))
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  for (uint32_t index = 0U; index < APP_WIFI_LWIP_PEER_CACHE_SIZE; index++)
  {
    const APP_WiFi_LwIP_PeerCacheEntry_t *entry = &g_peerCache[index];
    if ((entry->valid != 0U) && (entry->ipAddress == ipAddress))
    {
      memcpy(macAddress, entry->macAddress, APP_WIFI_MAC_ADDRESS_SIZE);
      if (ageMs != NULL)
      {
        *ageMs = (uint32_t)((nowTick - entry->lastSeenTick) * portTICK_PERIOD_MS);
      }
      found = 1U;
      break;
    }
  }
  taskEXIT_CRITICAL();

  return found;
}

/* Learn peer IP/MAC mapping from incoming Ethernet payload. */
static void APP_WiFi_LwIP_CachePeerFromFrame(const uint8_t *frame, uint16_t length)
{
  uint16_t etherType = 0U;
  uint32_t sourceIp = 0U;
  const uint8_t *sourceMac = NULL;

  if ((frame == NULL) || (length < 34U))
  {
    return;
  }

  etherType = (uint16_t)(((uint16_t)frame[12] << 8) | frame[13]);

  if ((etherType == 0x0800U) && (length >= 34U))
  {
    sourceIp = APP_WiFi_LwIP_BuildIpAddressU32(frame[26], frame[27], frame[28], frame[29]);
    sourceMac = &frame[6];
  }
  else if ((etherType == 0x0806U) && (length >= 42U))
  {
    sourceIp = APP_WiFi_LwIP_BuildIpAddressU32(frame[28], frame[29], frame[30], frame[31]);
    sourceMac = &frame[22];
  }

  if ((sourceIp != 0U) && (sourceMac != NULL))
  {
    APP_WiFi_LwIP_UpdatePeerCache(sourceIp, sourceMac);
  }
}

static void APP_WiFi_LwIP_LogClientEndpoint(const char *eventName,
                                            uint8_t slotIndex,
                                            uint8_t node,
                                            uint32_t ipAddress,
                                            uint16_t port,
                                            int32_t extraValue,
                                            const char *extraLabel)
{
  char ipBuffer[APP_WIFI_LWIP_IPV4_STR_LEN] = {0};
  char macBuffer[APP_WIFI_LWIP_MAC_STR_LEN] = {0};
  uint8_t macAddress[APP_WIFI_MAC_ADDRESS_SIZE] = {0U};
  uint32_t macAgeMs = 0U;
  uint8_t macKnown = APP_WiFi_LwIP_FindPeerMac(ipAddress, macAddress, &macAgeMs);

  APP_WiFi_LwIP_FormatIpAddress(ipAddress, ipBuffer, (uint32_t)sizeof(ipBuffer));
  if (macKnown != 0U)
  {
    APP_WiFi_LwIP_FormatMacAddress(macAddress, macBuffer, (uint32_t)sizeof(macBuffer));
  }
  else
  {
    APP_WiFi_LwIP_FormatMacAddress(NULL, macBuffer, (uint32_t)sizeof(macBuffer));
  }

  if ((extraLabel != NULL) && (extraLabel[0] != '\0'))
  {
    printf("[lwip] %s slot=%u node=%u ip=%s port=%u mac=%s mac_age=%lums %s=%ld\n",
           (eventName != NULL) ? eventName : "client",
           (unsigned int)slotIndex,
           (unsigned int)node,
           ipBuffer,
           (unsigned int)port,
           macBuffer,
           (unsigned long)macAgeMs,
           extraLabel,
           (long)extraValue);
  }
  else
  {
    printf("[lwip] %s slot=%u node=%u ip=%s port=%u mac=%s mac_age=%lums\n",
           (eventName != NULL) ? eventName : "client",
           (unsigned int)slotIndex,
           (unsigned int)node,
           ipBuffer,
           (unsigned int)port,
           macBuffer,
           (unsigned long)macAgeMs);
  }
}

/* lwIP netif linkoutput: flatten pbuf chain and forward Ethernet frame to WiFi driver queue. */
static err_t APP_WiFi_LwIP_LinkOutput(struct netif *netif, struct pbuf *p)
{
  uint8_t *copy = NULL;
  uint16_t packetLength = 0U;
  uint16_t bytesCopied = 0U;
  uint8_t priority = 0U;

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

  priority = APP_WiFi_LwIP_IsPriorityTxFrame(copy, packetLength);
  if (APP_WiFi_LwIP_QueuePush(copy, packetLength, priority) == 0U)
  {
    g_txEnqueueFailCount++;
    vPortFree(copy);
    if ((g_txEnqueueFailCount <= 8U) || ((g_txEnqueueFailCount % 32U) == 0U))
    {
      printf("[lwip] tx enqueue failed #%lu len=%u q=%u\n",
             (unsigned long)g_txEnqueueFailCount,
             (unsigned int)packetLength,
             (unsigned int)(g_txQueueCount + g_txPriorityQueueCount));
    }
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
           (unsigned int)(g_txQueueCount + g_txPriorityQueueCount));
    APP_WiFi_LwIP_LogTxFrame(copy, packetLength, g_linkOutputCount);
  }

  /* Keep SDIO TX serialized in the WiFi task; linkoutput may run in tcpip_thread. */
  return ERR_OK;
}

/* Initialize lwIP netif object for AP6181-backed station interface. */
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

/* Refresh lwIP netif hardware address from WiFi firmware MAC source of truth. */
static err_t APP_WiFi_LwIP_RefreshNetifMacAddress(struct netif *netif)
{
  uint8_t macAddress[APP_WIFI_MAC_ADDRESS_SIZE] = {0U};
  char ipBuffer[16] = {0};
  const char *ipText = "0.0.0.0";

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

  if (ip4addr_ntoa_r(netif_ip4_addr(netif), ipBuffer, sizeof(ipBuffer)) != NULL)
  {
    ipText = ipBuffer;
  }

  printf("[lwip] netif ip=%s mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
         ipText,
         (unsigned int)macAddress[0],
         (unsigned int)macAddress[1],
         (unsigned int)macAddress[2],
         (unsigned int)macAddress[3],
         (unsigned int)macAddress[4],
         (unsigned int)macAddress[5]);

  return ERR_OK;
}

#if (APP_WIFI_LWIP_GRATUITOUS_ARP_ENABLE != 0U)
/* Emit gratuitous ARP probe/update when explicitly requested. */
static err_t APP_WiFi_LwIP_SendGratuitousArp(struct netif *netif)
{
  if (netif == NULL)
  {
    return ERR_ARG;
  }

  return etharp_gratuitous(netif);
}

/* Conditionally refresh ARP announcement based on runtime policy. */
static void APP_WiFi_LwIP_MaybeRefreshArp(void)
{
  TickType_t nowTick = xTaskGetTickCount();
  err_t result = ERR_OK;

  if ((g_netifUp == 0U) ||
      (dhcp_supplied_address(&g_wifiNetif) == 0) ||
      (g_peerProtocolSeen != 0U) ||
      (g_arpRefreshCount >= APP_WIFI_LWIP_ARP_REFRESH_MAX) ||
      ((g_nextArpRefreshTick != 0U) &&
       ((int32_t)(nowTick - g_nextArpRefreshTick) < 0)))
  {
    return;
  }

  if ((g_txQueueCount + g_txPriorityQueueCount) != 0U)
  {
    /* Give normal traffic priority and avoid ARP frames filling the TX queue. */
    g_nextArpRefreshTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_ARP_REFRESH_MS);
    return;
  }

  result = netifapi_netif_common(&g_wifiNetif, NULL, APP_WiFi_LwIP_SendGratuitousArp);
  g_nextArpRefreshTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_ARP_REFRESH_MS);
  if (result == ERR_OK)
  {
    g_arpRefreshCount++;
    printf("[lwip] gratuitous arp #%lu waiting for peer\n", (unsigned long)g_arpRefreshCount);
  }
  else
  {
    printf("[lwip] gratuitous arp failed: %d\n", (int)result);
  }
}
#endif

/* Start DHCP client on netif and reset progress bookkeeping. */
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

/* Poll DHCP state machine and apply timeout/restart guards. */
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
           (unsigned int)(g_txQueueCount + g_txPriorityQueueCount));
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
           (unsigned int)(g_txQueueCount + g_txPriorityQueueCount));

    (void)netifapi_dhcp_stop(&g_wifiNetif);
    g_dhcpStarted = 0U;
    g_ipAnnounced = 0U;
    APP_WiFi_LwIP_DropQueuedTx();
    APP_WiFi_LwIP_StartDhcp();
  }
}

/* Classify server-to-node TCP payloads so controls do not sit behind pure ACK traffic. */
static uint8_t APP_WiFi_LwIP_IsPriorityTxFrame(const uint8_t *data, uint16_t length)
{
  uint16_t etherType = 0U;
  uint16_t ipHeaderLength = 0U;
  uint16_t ipTotalLength = 0U;
  uint16_t tcpOffset = 0U;
  uint16_t tcpHeaderLength = 0U;
  uint16_t tcpPayloadLength = 0U;
  uint16_t srcPort = 0U;

  if ((data == NULL) || (length < 54U))
  {
    return 0U;
  }

  etherType = (uint16_t)(((uint16_t)data[12] << 8) | data[13]);
  if ((etherType != 0x0800U) || (data[23] != 6U))
  {
    return 0U;
  }

  ipHeaderLength = (uint16_t)((data[14] & 0x0FU) * 4U);
  if ((ipHeaderLength < 20U) || (length < (uint16_t)(14U + ipHeaderLength + 20U)))
  {
    return 0U;
  }

  ipTotalLength = (uint16_t)(((uint16_t)data[16] << 8) | data[17]);
  if ((ipTotalLength < ipHeaderLength) || (length < (uint16_t)(14U + ipTotalLength)))
  {
    return 0U;
  }

  tcpOffset = (uint16_t)(14U + ipHeaderLength);
  tcpHeaderLength = (uint16_t)(((data[tcpOffset + 12U] >> 4) & 0x0FU) * 4U);
  if ((tcpHeaderLength < 20U) ||
      (ipTotalLength < (uint16_t)(ipHeaderLength + tcpHeaderLength)))
  {
    return 0U;
  }

  tcpPayloadLength = (uint16_t)(ipTotalLength - ipHeaderLength - tcpHeaderLength);
  srcPort = (uint16_t)(((uint16_t)data[tcpOffset] << 8) | data[tcpOffset + 1U]);

  return ((srcPort == APP_WIFI_LWIP_TCP_SERVER_PORT) && (tcpPayloadLength != 0U)) ? 1U : 0U;
}

/* Push one outgoing Ethernet frame into bounded TX queue. */
static uint8_t APP_WiFi_LwIP_QueuePush(uint8_t *data, uint16_t length, uint8_t priority)
{
  uint8_t *droppedData = NULL;
  uint16_t droppedLen = 0U;
  uint8_t queueCountAfter = 0U;

  if ((data == NULL) || (g_txQueueMutex == NULL))
  {
    return 0U;
  }

  if (xSemaphoreTake(g_txQueueMutex, portMAX_DELAY) != pdTRUE)
  {
    return 0U;
  }

  if (priority != 0U)
  {
    if (g_txPriorityQueueCount >= APP_WIFI_LWIP_TX_PRIORITY_QUEUE_DEPTH)
    {
      droppedData = g_txPriorityQueue[g_txPriorityQueueHead].data;
      droppedLen = g_txPriorityQueue[g_txPriorityQueueHead].length;
      g_txPriorityQueue[g_txPriorityQueueHead].data = NULL;
      g_txPriorityQueue[g_txPriorityQueueHead].length = 0U;
      g_txPriorityQueue[g_txPriorityQueueHead].priority = 0U;
      g_txPriorityQueueHead = (uint8_t)((g_txPriorityQueueHead + 1U) % APP_WIFI_LWIP_TX_PRIORITY_QUEUE_DEPTH);
      if (g_txPriorityQueueCount != 0U)
      {
        g_txPriorityQueueCount--;
      }
      g_txQueueDropOldestCount++;
    }

    g_txPriorityQueue[g_txPriorityQueueTail].data = data;
    g_txPriorityQueue[g_txPriorityQueueTail].length = length;
    g_txPriorityQueue[g_txPriorityQueueTail].priority = 1U;
    g_txPriorityQueueTail = (uint8_t)((g_txPriorityQueueTail + 1U) % APP_WIFI_LWIP_TX_PRIORITY_QUEUE_DEPTH);
    g_txPriorityQueueCount++;
  }
  else
  {
    if (g_txQueueCount >= APP_WIFI_LWIP_TX_QUEUE_DEPTH)
    {
      /*
       * Under bursty multi-node traffic, preserving newest frames gives better
       * control responsiveness than blocking all new egress behind stale backlog.
       */
      droppedData = g_txQueue[g_txQueueHead].data;
      droppedLen = g_txQueue[g_txQueueHead].length;
      g_txQueue[g_txQueueHead].data = NULL;
      g_txQueue[g_txQueueHead].length = 0U;
      g_txQueue[g_txQueueHead].priority = 0U;
      g_txQueueHead = (uint8_t)((g_txQueueHead + 1U) % APP_WIFI_LWIP_TX_QUEUE_DEPTH);
      if (g_txQueueCount != 0U)
      {
        g_txQueueCount--;
      }
      g_txQueueDropOldestCount++;
    }

    g_txQueue[g_txQueueTail].data = data;
    g_txQueue[g_txQueueTail].length = length;
    g_txQueue[g_txQueueTail].priority = 0U;
    g_txQueueTail = (uint8_t)((g_txQueueTail + 1U) % APP_WIFI_LWIP_TX_QUEUE_DEPTH);
    g_txQueueCount++;
  }

  queueCountAfter = (uint8_t)(g_txQueueCount + g_txPriorityQueueCount);

  (void)xSemaphoreGive(g_txQueueMutex);

  if (droppedData != NULL)
  {
    vPortFree(droppedData);
    if ((g_txQueueDropOldestCount <= 8U) || ((g_txQueueDropOldestCount % 32U) == 0U))
    {
      printf("[lwip] tx queue drop-oldest #%lu dropped_len=%u keep_len=%u q=%u\n",
             (unsigned long)g_txQueueDropOldestCount,
             (unsigned int)droppedLen,
             (unsigned int)length,
             (unsigned int)queueCountAfter);
    }
  }

  return 1U;
}

/* Peek next queued TX packet without removing it. */
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

  if ((g_txPriorityQueueCount == 0U) && (g_txQueueCount == 0U))
  {
    (void)xSemaphoreGive(g_txQueueMutex);
    return 0U;
  }

  if (g_txPriorityQueueCount != 0U)
  {
    *packet = g_txPriorityQueue[g_txPriorityQueueHead];
    packet->priority = 1U;
  }
  else
  {
    *packet = g_txQueue[g_txQueueHead];
    packet->priority = 0U;
  }
  (void)xSemaphoreGive(g_txQueueMutex);
  return 1U;
}

/* Pop and free the head entry after successful or dropped TX handling. */
static void APP_WiFi_LwIP_QueuePop(uint8_t priority)
{
  if (g_txQueueMutex == NULL)
  {
    return;
  }

  if (xSemaphoreTake(g_txQueueMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  if (priority != 0U)
  {
    if (g_txPriorityQueueCount != 0U)
    {
      g_txPriorityQueue[g_txPriorityQueueHead].data = NULL;
      g_txPriorityQueue[g_txPriorityQueueHead].length = 0U;
      g_txPriorityQueue[g_txPriorityQueueHead].priority = 0U;
      g_txPriorityQueueHead = (uint8_t)((g_txPriorityQueueHead + 1U) % APP_WIFI_LWIP_TX_PRIORITY_QUEUE_DEPTH);
      g_txPriorityQueueCount--;
    }
  }
  else if (g_txQueueCount != 0U)
  {
    g_txQueue[g_txQueueHead].data = NULL;
    g_txQueue[g_txQueueHead].length = 0U;
    g_txQueue[g_txQueueHead].priority = 0U;
    g_txQueueHead = (uint8_t)((g_txQueueHead + 1U) % APP_WIFI_LWIP_TX_QUEUE_DEPTH);
    g_txQueueCount--;
  }

  (void)xSemaphoreGive(g_txQueueMutex);
}

/* Lazily create control-path mutex protecting pending control queue. */
static uint8_t APP_WiFi_LwIP_EnsureControlMutex(void)
{
  if (g_controlMutex != NULL)
  {
    return 1U;
  }

  g_controlMutex = xSemaphoreCreateMutex();
  if (g_controlMutex != NULL)
  {
    APP_WiFi_LwIP_RegisterQueueForDebug((QueueHandle_t)g_controlMutex, "lwip_control_mutex");
  }
  return (g_controlMutex != NULL) ? 1U : 0U;
}

/* Reset control queue/reply tracking state after link/session reset. */
static void APP_WiFi_LwIP_ResetControlState(void)
{
  if ((APP_WiFi_LwIP_EnsureControlMutex() == 0U) ||
      (xSemaphoreTake(g_controlMutex, portMAX_DELAY) != pdTRUE))
  {
    return;
  }

  memset(g_pendingControls, 0, sizeof(g_pendingControls));
  memset(g_pendingControlValid, 0, sizeof(g_pendingControlValid));
  memset(g_pendingControlRetryCount, 0, sizeof(g_pendingControlRetryCount));
  memset(g_pendingControlReadyTick, 0, sizeof(g_pendingControlReadyTick));
  memset(g_nextControlTxTick, 0, sizeof(g_nextControlTxTick));
  g_nextGlobalControlTxTick = 0U;

  (void)xSemaphoreGive(g_controlMutex);
}

/* Store latest control intent per room with a short settle window. */
static uint8_t APP_WiFi_LwIP_StorePendingControlEx(const APP_WiFi_LwIP_ControlCommand_t *command,
                                                   uint8_t retryCount)
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
  g_pendingControlRetryCount[roomIndex] = retryCount;
  g_pendingControlReadyTick[roomIndex] = readyTick;
  g_nextControlTxTick[roomIndex] = 0U;

  (void)xSemaphoreGive(g_controlMutex);
  return 1U;
}

static uint8_t APP_WiFi_LwIP_StorePendingControl(const APP_WiFi_LwIP_ControlCommand_t *command)
{
  return APP_WiFi_LwIP_StorePendingControlEx(command, 0U);
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

/* Query whether lwIP netif + socket server are currently considered online. */
uint8_t APP_WiFi_LwIP_IsNetworkOnline(void)
{
  if ((g_netifAdded == 0U) || (g_netifUp == 0U) || (g_dhcpStarted == 0U))
  {
    return 0U;
  }

  return (dhcp_supplied_address(&g_wifiNetif) != 0) ? 1U : 0U;
}

/* Return non-zero when Ethernet TX path still has queued data. */
uint8_t APP_WiFi_LwIP_HasPendingTx(void)
{
  uint8_t hasPending = 0U;

  if (g_txQueueMutex == NULL)
  {
    return 0U;
  }

  if (xSemaphoreTake(g_txQueueMutex, pdMS_TO_TICKS(1U)) != pdTRUE)
  {
    return 1U;
  }

  hasPending = ((g_txQueueCount + g_txPriorityQueueCount) != 0U) ? 1U : 0U;
  (void)xSemaphoreGive(g_txQueueMutex);
  return hasPending;
}

/* Return total number of processed inbound Ethernet frames. */
uint32_t APP_WiFi_LwIP_GetRxEthernetFrameCount(void)
{
  return g_rxEthernetFrameCount;
}

/* Snapshot lightweight lwIP runtime load indicators for recovery diagnostics. */
void APP_WiFi_LwIP_GetRuntimeStats(APP_WiFi_LwIP_RuntimeStats_t *stats)
{
  uint16_t activeClients = 0U;
  uint16_t closePendingClients = 0U;
  uint16_t txQueueDepth = 0U;
  uint16_t pendingControls = 0U;

  if (stats == NULL)
  {
    return;
  }

  memset(stats, 0, sizeof(*stats));

  if ((g_clientSlotsMutex != NULL) &&
      (xSemaphoreTake(g_clientSlotsMutex, pdMS_TO_TICKS(2U)) == pdTRUE))
  {
    for (uint32_t index = 0U; index < APP_WIFI_LWIP_MAX_CLIENTS; index++)
    {
      const APP_WiFi_LwIP_ClientSlot_t *slot = &g_clientSlots[index];
      if (slot->active != 0U)
      {
        activeClients++;
        if (slot->closeRequested != 0U)
        {
          closePendingClients++;
        }
      }
    }
    (void)xSemaphoreGive(g_clientSlotsMutex);
  }

  if ((g_txQueueMutex != NULL) &&
      (xSemaphoreTake(g_txQueueMutex, pdMS_TO_TICKS(2U)) == pdTRUE))
  {
    txQueueDepth = (uint16_t)(g_txQueueCount + g_txPriorityQueueCount);
    (void)xSemaphoreGive(g_txQueueMutex);
  }

  if ((g_controlMutex != NULL) &&
      (xSemaphoreTake(g_controlMutex, pdMS_TO_TICKS(2U)) == pdTRUE))
  {
    for (uint32_t index = 0U; index < APP_WIFI_LWIP_CONTROL_ROOM_COUNT; index++)
    {
      if (g_pendingControlValid[index] != 0U)
      {
        pendingControls++;
      }
    }
    (void)xSemaphoreGive(g_controlMutex);
  }

  stats->activeClients = activeClients;
  stats->closePendingClients = closePendingClients;
  stats->txQueueDepth = txQueueDepth;
  stats->pendingControls = pendingControls;
  stats->inflightControls = 0U;
  stats->taskCount = (uint32_t)uxTaskGetNumberOfTasks();
  stats->freeHeapBytes = (uint32_t)xPortGetFreeHeapSize();
  stats->minEverFreeHeapBytes = (uint32_t)xPortGetMinimumEverFreeHeapSize();
  stats->txEnqueueFailCount = g_txEnqueueFailCount;
  stats->rxPbufAllocFailCount = g_rxPbufAllocFailCount;
  stats->rxTcpipInputFailCount = g_rxTcpipInputFailCount;
}

/* Print one compact runtime-load snapshot for control-path debugging. */
static void APP_WiFi_LwIP_LogRuntimeStats(const char *source)
{
  APP_WiFi_LwIP_RuntimeStats_t stats;

  memset(&stats, 0, sizeof(stats));
  APP_WiFi_LwIP_GetRuntimeStats(&stats);

  printf("[lwip] runtime source=%s clients=%u closing=%u txq=%u pending=%u inflight=%u tasks=%lu heap=%lu min_heap=%lu txq_fail=%lu rx_pbuf_fail=%lu rx_input_fail=%lu\n",
         (source != NULL) ? source : "unknown",
         (unsigned int)stats.activeClients,
         (unsigned int)stats.closePendingClients,
         (unsigned int)stats.txQueueDepth,
         (unsigned int)stats.pendingControls,
         (unsigned int)stats.inflightControls,
         (unsigned long)stats.taskCount,
         (unsigned long)stats.freeHeapBytes,
         (unsigned long)stats.minEverFreeHeapBytes,
         (unsigned long)stats.txEnqueueFailCount,
         (unsigned long)stats.rxPbufAllocFailCount,
         (unsigned long)stats.rxTcpipInputFailCount);
}

/* Force-close all node sessions so clients reconnect on a clean TCP state. */
void APP_WiFi_LwIP_RequestSessionRefresh(void)
{
  APP_WiFi_LwIP_RequestCloseAllClients();
  APP_WiFi_LwIP_ResetControlState();
  APP_WiFi_LwIP_DropQueuedTx();
  g_peerProtocolSeen = 0U;
  g_serverRebuildRequested = 1U;
  printf("[lwip] session refresh requested\n");
}

/* Fully bounce lwIP netif state after WiFi link churn while keeping stack objects alive. */
void APP_WiFi_LwIP_RequestNetworkRebind(void)
{
  APP_WiFi_LwIP_RequestCloseAllClients();
  APP_WiFi_LwIP_ResetControlState();
  APP_WiFi_LwIP_DropQueuedTx();

  if ((g_netifAdded != 0U) && (g_netifUp != 0U))
  {
    if (g_dhcpStarted != 0U)
    {
      (void)netifapi_dhcp_stop(&g_wifiNetif);
    }

    (void)netifapi_netif_set_link_down(&g_wifiNetif);
    (void)netifapi_netif_set_down(&g_wifiNetif);
  }

  g_netifUp = 0U;
  g_netifMacSynced = 0U;
  g_dhcpStarted = 0U;
  g_ipAnnounced = 0U;
  g_peerProtocolSeen = 0U;
  g_dhcpStartTick = 0U;
  g_nextDhcpWaitLogTick = 0U;
  g_dhcpRestartCount = 0U;
  g_nextArpRefreshTick = 0U;
  g_arpRefreshCount = 0U;
  memset(g_peerCache, 0, sizeof(g_peerCache));
  g_serverRebuildRequested = 1U;
  printf("[lwip] network rebind requested\n");
}

/* Queue/merge one outbound CONTROL command for target node (room-mapped). */
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
  APP_WiFi_LwIP_LogRuntimeStats("control-queued");
#if (APP_WIFI_LWIP_INFO_LOG_ENABLE != 0U)
  printf("[proto] control queued #%lu node=%u seq=%u target_x10=%d mode=%u fan=%u flags=0x%02X settle=%ums\n",
         (unsigned long)g_serverControlQueuedCount,
         (unsigned int)command.node,
         (unsigned int)command.sequence,
         (int)command.targetTemperature_x10,
         (unsigned int)command.mode,
         (unsigned int)command.fan,
         (unsigned int)command.flags,
         (unsigned int)APP_WIFI_LWIP_CONTROL_DEBOUNCE_MS);
#endif

  return 1U;
}

/* Drop queued outbound frames during hard network reset/reinit. */
static void APP_WiFi_LwIP_DropQueuedTx(void)
{
  APP_WiFi_LwIP_TxPacket_t packet = {0};

  for (;;)
  {
    if (APP_WiFi_LwIP_QueuePeek(&packet) == 0U)
    {
      break;
    }

    APP_WiFi_LwIP_QueuePop(packet.priority);
    if (packet.data != NULL)
    {
      vPortFree(packet.data);
    }
  }
}

/* Try to transmit queued frames while netif is online; keep ordering stable. */
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
      g_txHeadErrorStreak = 0U;
      g_txLastErrorHeadData = NULL;
      g_txLastErrorHeadLen = 0U;
      if ((APP_WIFI_LWIP_ETH_TRACE_ENABLE != 0U) && (g_txOkCount <= 32U))
      {
        printf("[lwip] tx ok #%lu len=%u q=%u\n",
               (unsigned long)g_txOkCount,
               (unsigned int)packet.length,
               (unsigned int)(g_txQueueCount + g_txPriorityQueueCount));
      }

      APP_WiFi_LwIP_QueuePop(packet.priority);
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
      if ((packet.data == g_txLastErrorHeadData) &&
          (packet.length == g_txLastErrorHeadLen))
      {
        if (g_txHeadErrorStreak < 0xFFU)
        {
          g_txHeadErrorStreak++;
        }
      }
      else
      {
        g_txLastErrorHeadData = packet.data;
        g_txLastErrorHeadLen = packet.length;
        g_txHeadErrorStreak = 1U;
      }

      if (g_txHeadErrorStreak >= APP_WIFI_LWIP_TX_ERROR_RETRY_BEFORE_DROP)
      {
        printf("[lwip] dropping queued frame len=%u after tx error streak=%u\n",
               (unsigned int)packet.length,
               (unsigned int)g_txHeadErrorStreak);
        APP_WiFi_LwIP_QueuePop(packet.priority);
        if (packet.data != NULL)
        {
          vPortFree(packet.data);
        }
        g_txHeadErrorStreak = 0U;
        g_txLastErrorHeadData = NULL;
        g_txLastErrorHeadLen = 0U;
      }
      else
      {
        if ((g_txHeadErrorStreak <= 4U) || ((g_txHeadErrorStreak % 4U) == 0U))
        {
          printf("[lwip] tx error retry head len=%u streak=%u/%u\n",
                 (unsigned int)packet.length,
                 (unsigned int)g_txHeadErrorStreak,
                 (unsigned int)APP_WIFI_LWIP_TX_ERROR_RETRY_BEFORE_DROP);
        }
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
                 (unsigned int)(g_txQueueCount + g_txPriorityQueueCount));
      }
    }

    break;
  }

  (void)xSemaphoreGive(g_txFlushMutex);
}

/* Optional RX frame logger used for low-level transport troubleshooting. */
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

/* Optional TX frame logger used for low-level transport troubleshooting. */
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

/* Dump bounded binary payload as hex string with caller prefix. */
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

/* Blocking helper to send full payload over one lwIP socket. */
static uint8_t APP_WiFi_LwIP_SendAll(int socketHandle, const uint8_t *data, uint16_t length)
{
  uint16_t offset = 0U;
  uint32_t wouldBlockRetries = 0U;

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
    if (sent > 0)
    {
      offset = (uint16_t)(offset + (uint16_t)sent);
      wouldBlockRetries = 0U;
      continue;
    }

    if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
    {
      /*
       * Keep socket sends non-blocking. A blocked control send is requeued by
       * the caller so the WiFi task can continue draining SDIO RX/TX.
       */
      wouldBlockRetries++;
      if (wouldBlockRetries > APP_WIFI_LWIP_SEND_WOULDBLOCK_RETRY_MAX)
      {
        g_serverSendErrorCount++;
        if ((g_serverSendErrorCount <= 8U) || ((g_serverSendErrorCount % 32U) == 0U))
        {
          printf("[proto] send blocked too long #%lu after=%u len=%u\n",
                 (unsigned long)g_serverSendErrorCount,
                 (unsigned int)offset,
                 (unsigned int)length);
        }
        return 0U;
      }

      vTaskDelay(pdMS_TO_TICKS(1U));
      continue;
    }

    if (sent <= 0)
    {
      g_serverSendErrorCount++;
      printf("[proto] send failed #%lu after=%u len=%u\n",
             (unsigned long)g_serverSendErrorCount,
             (unsigned int)offset,
             (unsigned int)length);
      return 0U;
    }
  }

  return 1U;
}

/* Force-close socket with abortive behavior so peer immediately sees disconnect. */
static void APP_WiFi_LwIP_AbortSocket(int socketHandle)
{
  struct linger lingerOption;

  if (socketHandle < 0)
  {
    return;
  }

  lingerOption.l_onoff = 1;
  lingerOption.l_linger = 0;
  (void)setsockopt(socketHandle, SOL_SOCKET, SO_LINGER, &lingerOption, (socklen_t)sizeof(lingerOption));
  (void)shutdown(socketHandle, SHUT_RDWR);
  closesocket(socketHandle);
}

/* Serialize and send one control command frame to selected node socket. */
static uint8_t APP_WiFi_LwIP_SendControlFrame(int clientSocket, const APP_WiFi_LwIP_ControlCommand_t *command)
{
  /*
   * Encode one CONTROL command into protocol frame and send atomically.
   * Socket send success only means bytes left lwIP; final apply is reflected by next telemetry state.
   */
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

/* Find the currently active socket for a node without holding slot lock during send. */
static uint8_t APP_WiFi_LwIP_FindActiveNodeSocket(uint8_t node, int *socketHandle)
{
  int foundSocket = -1;

  if ((socketHandle == NULL) ||
      (APP_WiFi_LwIP_IsKnownNode(node) == 0U) ||
      (APP_WiFi_LwIP_EnsureClientSlotsMutex() == 0U))
  {
    return 0U;
  }

  if (xSemaphoreTake(g_clientSlotsMutex, pdMS_TO_TICKS(1U)) != pdTRUE)
  {
    return 0U;
  }

  for (uint32_t index = 0U; index < APP_WIFI_LWIP_MAX_CLIENTS; index++)
  {
    const APP_WiFi_LwIP_ClientSlot_t *slot = &g_clientSlots[index];

    if ((slot->active != 0U) &&
        (slot->closeRequested == 0U) &&
        (slot->node == node) &&
        (slot->socketHandle >= 0))
    {
      foundSocket = slot->socketHandle;
      break;
    }
  }

  (void)xSemaphoreGive(g_clientSlotsMutex);

  if (foundSocket < 0)
  {
    return 0U;
  }

  *socketHandle = foundSocket;
  return 1U;
}

/* Service pending controls from the WiFi/lwIP service loop so client RX load cannot starve TX. */
static void APP_WiFi_LwIP_DrainAllPendingControls(void)
{
  const TickType_t nowTick = xTaskGetTickCount();

  if ((APP_WiFi_LwIP_IsNetworkOnline() == 0U) ||
      (APP_WiFi_LwIP_EnsureControlMutex() == 0U))
  {
    return;
  }

  if (xSemaphoreTake(g_controlMutex, pdMS_TO_TICKS(1U)) != pdTRUE)
  {
    return;
  }
  if ((g_nextGlobalControlTxTick != 0U) &&
      ((int32_t)(nowTick - g_nextGlobalControlTxTick) < 0))
  {
    (void)xSemaphoreGive(g_controlMutex);
    return;
  }
  (void)xSemaphoreGive(g_controlMutex);

  for (uint8_t roomIndex = 0U; roomIndex < APP_WIFI_LWIP_CONTROL_ROOM_COUNT; roomIndex++)
  {
    APP_WiFi_LwIP_ControlCommand_t command = {0};
    int clientSocket = -1;
    uint8_t hasCommand = 0U;

    if (xSemaphoreTake(g_controlMutex, pdMS_TO_TICKS(1U)) != pdTRUE)
    {
      return;
    }

    if ((g_nextGlobalControlTxTick != 0U) &&
        ((int32_t)(nowTick - g_nextGlobalControlTxTick) < 0))
    {
      (void)xSemaphoreGive(g_controlMutex);
      return;
    }

    if ((g_nextControlTxTick[roomIndex] != 0U) &&
        ((int32_t)(nowTick - g_nextControlTxTick[roomIndex]) < 0))
    {
      (void)xSemaphoreGive(g_controlMutex);
      continue;
    }

    if ((g_pendingControlValid[roomIndex] != 0U) &&
        ((int32_t)(nowTick - g_pendingControlReadyTick[roomIndex]) >= 0))
    {
      command = g_pendingControls[roomIndex];
      g_pendingControlValid[roomIndex] = 0U;
      g_pendingControlRetryCount[roomIndex] = 0U;
      g_pendingControlReadyTick[roomIndex] = 0U;
      g_nextGlobalControlTxTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_GLOBAL_TX_GAP_MS);
      hasCommand = 1U;
    }

    (void)xSemaphoreGive(g_controlMutex);

    if (hasCommand == 0U)
    {
      continue;
    }

    if (APP_WiFi_LwIP_FindActiveNodeSocket(command.node, &clientSocket) == 0U)
    {
      if (xSemaphoreTake(g_controlMutex, pdMS_TO_TICKS(1U)) == pdTRUE)
      {
        g_pendingControls[roomIndex] = command;
        g_pendingControlValid[roomIndex] = 1U;
        g_pendingControlReadyTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_RETRY_MS);
        g_nextControlTxTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_RETRY_MS);
        (void)xSemaphoreGive(g_controlMutex);
      }
      continue;
    }

    APP_WiFi_LwIP_FlushTxQueue();
    if (APP_WiFi_LwIP_SendControlFrame(clientSocket, &command) != 0U)
    {
      APP_WiFi_LwIP_FlushTxQueue();
      if (xSemaphoreTake(g_controlMutex, pdMS_TO_TICKS(1U)) == pdTRUE)
      {
        g_nextControlTxTick[roomIndex] = 0U;
        g_nextGlobalControlTxTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_GLOBAL_TX_GAP_MS);
        (void)xSemaphoreGive(g_controlMutex);
      }
    }
    else
    {
      if (xSemaphoreTake(g_controlMutex, pdMS_TO_TICKS(1U)) == pdTRUE)
      {
        g_pendingControls[roomIndex] = command;
        g_pendingControlValid[roomIndex] = 1U;
        g_pendingControlRetryCount[roomIndex] = 0U;
        g_pendingControlReadyTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_RETRY_MS);
        g_nextControlTxTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_RETRY_MS);
        g_nextGlobalControlTxTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_GLOBAL_TX_GAP_MS);
        (void)xSemaphoreGive(g_controlMutex);
      }
    }
  }
}

/* Try sending queued controls whose destination matches active client node. */
static void APP_WiFi_LwIP_DrainPendingControls(int clientSocket, uint8_t clientNode)
{
  /* Latest-intent control scheduler without protocol ACK dependency. */
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

  if ((g_nextControlTxTick[roomIndex] != 0U) &&
      ((int32_t)(nowTick - g_nextControlTxTick[roomIndex]) < 0))
  {
    (void)xSemaphoreGive(g_controlMutex);
    return;
  }

  if ((g_nextGlobalControlTxTick != 0U) &&
      ((int32_t)(nowTick - g_nextGlobalControlTxTick) < 0))
  {
    (void)xSemaphoreGive(g_controlMutex);
    return;
  }

  if ((g_pendingControlValid[roomIndex] != 0U) &&
      ((int32_t)(nowTick - g_pendingControlReadyTick[roomIndex]) >= 0))
  {
    command = g_pendingControls[roomIndex];
    g_pendingControlValid[roomIndex] = 0U;
    g_pendingControlRetryCount[roomIndex] = 0U;
    g_pendingControlReadyTick[roomIndex] = 0U;
    g_nextGlobalControlTxTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_GLOBAL_TX_GAP_MS);
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
      /* Telemetry will prove the final state; avoid ACK/confirm retries that amplify load. */
      g_nextControlTxTick[roomIndex] = 0U;
      g_nextGlobalControlTxTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_GLOBAL_TX_GAP_MS);
      (void)xSemaphoreGive(g_controlMutex);
    }
  }
  else
  {
    if (xSemaphoreTake(g_controlMutex, portMAX_DELAY) == pdTRUE)
    {
      g_pendingControls[roomIndex] = command;
      g_pendingControlValid[roomIndex] = 1U;
      g_pendingControlRetryCount[roomIndex] = 0U;
      g_pendingControlReadyTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_RETRY_MS);
      g_nextControlTxTick[roomIndex] = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_RETRY_MS);
      g_nextGlobalControlTxTick = nowTick + pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_GLOBAL_TX_GAP_MS);
      (void)xSemaphoreGive(g_controlMutex);
    }
  }
}

/* ACK/ERR frames are intentionally ignored; telemetry is the source of truth. */
static void APP_WiFi_LwIP_HandleControlReply(const APP_HomeProtocolFrame_t *frame, uint8_t isError)
{
  (void)frame;
  (void)isError;
}

/* Release any pending command immediately when a client reconnects. */
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

  if (g_pendingControlValid[roomIndex] != 0U)
  {
    g_pendingControlReadyTick[roomIndex] = nowTick;
    g_nextControlTxTick[roomIndex] = 0U;
  }

  (void)xSemaphoreGive(g_controlMutex);
}

/* Send a lightweight protocol ACK for peer frames that expect flow confirmation. */
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
#if (APP_WIFI_LWIP_INFO_LOG_ENABLE != 0U)
    printf("[proto] ack #%lu node=%u seq=%u cmd=%s\n",
           (unsigned long)g_serverAckCount,
           (unsigned int)frame->node,
           (unsigned int)frame->sequence,
           APP_HomeProtocol_CommandToString(frame->command));
#endif
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

/* Validate protocol command byte against implemented command set. */
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

/* Validate node id byte against supported room-node range. */
static uint8_t APP_WiFi_LwIP_IsKnownNode(uint8_t node)
{
  uint8_t roomIndex = 0U;
  return APP_HomeData_NodeToRoomIndex(node, &roomIndex);
}

/* Read little-endian 16-bit value from protocol buffer. */
static uint16_t APP_WiFi_LwIP_ReadLe16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

/* Read signed little-endian 16-bit telemetry scalar from payload. */
static int16_t APP_WiFi_LwIP_ReadI16(const uint8_t *data)
{
  return (int16_t)APP_WiFi_LwIP_ReadLe16(data);
}

/* Lazily create mutex guarding multi-client slot table. */
static uint8_t APP_WiFi_LwIP_EnsureClientSlotsMutex(void)
{
  if (g_clientSlotsMutex != NULL)
  {
    return 1U;
  }

  g_clientSlotsMutex = xSemaphoreCreateMutex();
  if (g_clientSlotsMutex != NULL)
  {
    APP_WiFi_LwIP_RegisterQueueForDebug((QueueHandle_t)g_clientSlotsMutex, "lwip_slots_mutex");
  }
  return (g_clientSlotsMutex != NULL) ? 1U : 0U;
}

/* Reserve one client slot for an accepted socket and initialize slot metadata. */
static int32_t APP_WiFi_LwIP_ClaimClientSlot(uint32_t ipAddress, int socketHandle)
{
  int32_t freeIndex = -1;

  if ((socketHandle < 0) || (APP_WiFi_LwIP_EnsureClientSlotsMutex() == 0U))
  {
    return -1;
  }

  if (xSemaphoreTake(g_clientSlotsMutex, portMAX_DELAY) != pdTRUE)
  {
    return -1;
  }

  for (uint32_t index = 0U; index < APP_WIFI_LWIP_MAX_CLIENTS; index++)
  {
    APP_WiFi_LwIP_ClientSlot_t *slot = &g_clientSlots[index];

    if ((slot->active != 0U) && (slot->ipAddress == ipAddress))
    {
      slot->closeRequested = 1U;
      APP_WiFi_LwIP_LogClientEndpoint("close same-ip old",
                                      (uint8_t)index,
                                      slot->node,
                                      slot->ipAddress,
                                      0U,
                                      (int32_t)slot->socketHandle,
                                      "socket");
    }

    if ((freeIndex < 0) && (slot->active == 0U))
    {
      freeIndex = (int32_t)index;
    }
  }

  if (freeIndex >= 0)
  {
    APP_WiFi_LwIP_ClientSlot_t *slot = &g_clientSlots[(uint32_t)freeIndex];
    slot->socketHandle = socketHandle;
    slot->ipAddress = ipAddress;
    slot->node = APP_HOME_NODE_NONE;
    slot->active = 1U;
    slot->closeRequested = 0U;
  }

  (void)xSemaphoreGive(g_clientSlotsMutex);
  return freeIndex;
}

/* Enforce single-active-socket ownership per node id across all client slots. */
static void APP_WiFi_LwIP_RequestNodeTakeover(uint8_t slotIndex, uint8_t node)
{
  uint8_t previousNode = APP_HOME_NODE_NONE;
  uint32_t ipAddress = 0U;

  if ((APP_WiFi_LwIP_IsKnownNode(node) == 0U) ||
      (slotIndex >= APP_WIFI_LWIP_MAX_CLIENTS) ||
      (APP_WiFi_LwIP_EnsureClientSlotsMutex() == 0U))
  {
    return;
  }

  if (xSemaphoreTake(g_clientSlotsMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  if (g_clientSlots[slotIndex].active == 0U)
  {
    (void)xSemaphoreGive(g_clientSlotsMutex);
    return;
  }

  previousNode = g_clientSlots[slotIndex].node;
  ipAddress = g_clientSlots[slotIndex].ipAddress;
  g_clientSlots[slotIndex].node = node;

  if (previousNode != node)
  {
    APP_WiFi_LwIP_LogClientEndpoint("slot bind",
                                    slotIndex,
                                    node,
                                    ipAddress,
                                    0U,
                                    (int32_t)previousNode,
                                    "prev_node");
  }

  for (uint32_t index = 0U; index < APP_WIFI_LWIP_MAX_CLIENTS; index++)
  {
    APP_WiFi_LwIP_ClientSlot_t *slot = &g_clientSlots[index];

    if ((index == slotIndex) || (slot->active == 0U))
    {
      continue;
    }

    if ((slot->node == node) && (slot->closeRequested == 0U))
    {
      slot->closeRequested = 1U;
      printf("[lwip] node takeover node=%u close slot=%lu keep slot=%u\n",
             (unsigned int)node,
             (unsigned long)index,
             (unsigned int)slotIndex);
    }
  }

  (void)xSemaphoreGive(g_clientSlotsMutex);
}

/* Check whether one client slot has pending forced-close request. */
static uint8_t APP_WiFi_LwIP_IsCloseRequested(uint8_t slotIndex)
{
  uint8_t closeRequested = 1U;

  if ((slotIndex >= APP_WIFI_LWIP_MAX_CLIENTS) ||
      (APP_WiFi_LwIP_EnsureClientSlotsMutex() == 0U))
  {
    return 1U;
  }

  if (xSemaphoreTake(g_clientSlotsMutex, portMAX_DELAY) != pdTRUE)
  {
    return 1U;
  }

  if (g_clientSlots[slotIndex].active != 0U)
  {
    closeRequested = g_clientSlots[slotIndex].closeRequested;
  }

  (void)xSemaphoreGive(g_clientSlotsMutex);
  return closeRequested;
}

/* Detect duplicated node sessions occupying different client slots. */
static uint8_t APP_WiFi_LwIP_HasOtherActiveNodeClient(uint8_t slotIndex, uint8_t node)
{
  uint8_t hasOther = 0U;

  if ((APP_WiFi_LwIP_IsKnownNode(node) == 0U) ||
      (APP_WiFi_LwIP_EnsureClientSlotsMutex() == 0U))
  {
    return 0U;
  }

  if (xSemaphoreTake(g_clientSlotsMutex, portMAX_DELAY) != pdTRUE)
  {
    return 0U;
  }

  for (uint32_t index = 0U; index < APP_WIFI_LWIP_MAX_CLIENTS; index++)
  {
    const APP_WiFi_LwIP_ClientSlot_t *slot = &g_clientSlots[index];

    if ((index == slotIndex) || (slot->active == 0U))
    {
      continue;
    }

    if ((slot->node == node) && (slot->closeRequested == 0U))
    {
      hasOther = 1U;
      break;
    }
  }

  (void)xSemaphoreGive(g_clientSlotsMutex);
  return hasOther;
}

/* Fetch stable snapshot of one client slot under slot-table mutex. */
static uint8_t APP_WiFi_LwIP_GetSlotSnapshot(uint8_t slotIndex, uint32_t *ipAddress, uint8_t *node)
{
  uint8_t valid = 0U;

  if ((slotIndex >= APP_WIFI_LWIP_MAX_CLIENTS) ||
      (APP_WiFi_LwIP_EnsureClientSlotsMutex() == 0U))
  {
    return 0U;
  }

  if (xSemaphoreTake(g_clientSlotsMutex, portMAX_DELAY) != pdTRUE)
  {
    return 0U;
  }

  if (g_clientSlots[slotIndex].active != 0U)
  {
    if (ipAddress != NULL)
    {
      *ipAddress = g_clientSlots[slotIndex].ipAddress;
    }
    if (node != NULL)
    {
      *node = g_clientSlots[slotIndex].node;
    }
    valid = 1U;
  }

  (void)xSemaphoreGive(g_clientSlotsMutex);
  return valid;
}

/* Release one client slot and clear ownership/state metadata. */
static void APP_WiFi_LwIP_ReleaseClientSlot(uint8_t slotIndex)
{
  if ((slotIndex >= APP_WIFI_LWIP_MAX_CLIENTS) ||
      (APP_WiFi_LwIP_EnsureClientSlotsMutex() == 0U))
  {
    return;
  }

  if (xSemaphoreTake(g_clientSlotsMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  memset(&g_clientSlots[slotIndex], 0, sizeof(g_clientSlots[slotIndex]));
  g_clientSlots[slotIndex].socketHandle = -1;
  (void)xSemaphoreGive(g_clientSlotsMutex);
}

/* Mark all client slots for close during global reconnect/recovery. */
static void APP_WiFi_LwIP_RequestCloseAllClients(void)
{
  /*
   * Netif-down fence:
   * asynchronously ask every active client task to exit so sockets are not
   * left in half-open state across WiFi rejoin/rebind.
   */
  if (APP_WiFi_LwIP_EnsureClientSlotsMutex() == 0U)
  {
    return;
  }

  if (xSemaphoreTake(g_clientSlotsMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  for (uint32_t index = 0U; index < APP_WIFI_LWIP_MAX_CLIENTS; index++)
  {
    if (g_clientSlots[index].active != 0U)
    {
      const uint8_t node = g_clientSlots[index].node;
      g_clientSlots[index].closeRequested = 1U;
      APP_WiFi_LwIP_LogClientEndpoint("request close (netif down)",
                                      (uint8_t)index,
                                      node,
                                      g_clientSlots[index].ipAddress,
                                      0U,
                                      (int32_t)g_clientSlots[index].socketHandle,
                                      "socket");
      if (APP_WiFi_LwIP_IsKnownNode(node) != 0U)
      {
        (void)APP_HomeData_UpdateNodeOnline(node, 0U);
      }
    }
  }

  (void)xSemaphoreGive(g_clientSlotsMutex);
}

/* Decode and process one complete protocol frame received from a TCP client. */
static void APP_WiFi_LwIP_HandleProtocolFrame(int clientSocket,
                                              uint8_t slotIndex,
                                              const APP_HomeProtocolFrame_t *frame,
                                              uint8_t *clientNode)
{
  /*
   * Per-frame protocol dispatcher for one connected node:
   * 1) bind node identity to the current TCP slot
   * 2) validate command/payload
   * 3) push telemetry/control data into shared model state
   * 4) ACK only handshake/control commands; keep periodic reports one-way
   */
  APP_HomeProtocolError_t error = APP_HOME_ERR_NONE;
  uint8_t shouldAck = 0U;

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
#if ((APP_WIFI_LWIP_INFO_LOG_ENABLE != 0U) && (APP_WIFI_LWIP_GRATUITOUS_ARP_ENABLE != 0U))
    printf("[lwip] peer protocol seen, stop gratuitous arp\n");
#endif
  }

  if (APP_WiFi_LwIP_IsKnownNode(frame->node) != 0U)
  {
    if (clientNode != NULL)
    {
      *clientNode = frame->node;
    }

    APP_WiFi_LwIP_RequestNodeTakeover(slotIndex, frame->node);
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
      shouldAck = 1U;
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
#if (APP_WIFI_LWIP_INFO_LOG_ENABLE != 0U)
        printf("[proto] hello node=%u nameLen=%u\n",
               (unsigned int)frame->node,
               (unsigned int)frame->length);
#endif
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
        /*
         * Online state should be driven by actual socket/session liveness on the
         * upper node side. Some node firmware payloads may transiently report 0
         * in telemetry[6], which causes UI online flicker despite an active TCP
         * session. Keep telemetry from a connected client as online=1 here.
         */
        (void)APP_HomeData_UpdateTelemetry(frame->node,
                                           frame->sequence,
                                           temperature,
                                           humidity,
                                           frame->payload[4],
                                           frame->payload[5],
                                           1U,
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
      }
      break;

    case APP_HOME_CMD_CONTROL:
      shouldAck = 1U;
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
#if (APP_WIFI_LWIP_INFO_LOG_ENABLE != 0U)
        printf("[proto] control node=%u target_x10=%d mode=%u fan=%u flags=0x%02X\n",
               (unsigned int)frame->node,
               (int)target,
               (unsigned int)frame->payload[2],
               (unsigned int)frame->payload[3],
               (unsigned int)frame->payload[4]);
#else
        (void)target;
#endif
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
      APP_WiFi_LwIP_HandleControlReply(frame, 0U);
      break;

    case APP_HOME_CMD_ERR:
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

/* Launch TCP server task once after lwIP initialization completes. */
static void APP_WiFi_LwIP_StartServerTask(void)
{
  if (g_serverStarted != 0U)
  {
    return;
  }

  if (sys_thread_new("wifi_tcp", APP_WiFi_LwIP_ServerTask, NULL, APP_WIFI_LWIP_SOCKET_STACK_BYTES, (int)APP_WIFI_LWIP_SOCKET_PRIORITY) == NULL)
  {
    printf("[lwip] failed to start TCP server thread free_heap=%lu\n",
           (unsigned long)xPortGetFreeHeapSize());
    return;
  }

  g_serverStarted = 1U;
}

/* Per-client worker thread handling bidirectional protocol traffic for one socket. */
static void APP_WiFi_LwIP_ClientTask(void *argument)
{
  /*
   * One TCP client task per accepted socket. The task continuously:
   * - drains pending outbound controls for this node
   * - receives stream bytes and feeds the frame parser
   * - updates node online state on valid protocol activity
   * - exits only on close request, recv error, or peer close
   */
  APP_WiFi_LwIP_ClientContext_t *context = (APP_WiFi_LwIP_ClientContext_t *)argument;
  APP_HomeProtocolParser_t parser;
  APP_HomeProtocolFrame_t frame;
  uint8_t clientNode = APP_HOME_NODE_NONE;
  uint8_t slotIndex = 0U;
  int clientSocket = -1;
  uint32_t clientIpAddress = 0U;
  uint16_t clientPort = 0U;
  uint8_t abortiveClose = 0U;
#if (APP_WIFI_LWIP_CLIENT_IDLE_TIMEOUT_MS > 0U)
  TickType_t lastRxTick = xTaskGetTickCount();
#endif

  if (context == NULL)
  {
    vTaskDelete(NULL);
    return;
  }

  clientSocket = context->socketHandle;
  slotIndex = context->slotIndex;
  clientIpAddress = context->ipAddress;
  clientPort = context->port;
  vPortFree(context);

  if (clientIpAddress == 0U)
  {
    (void)APP_WiFi_LwIP_GetSlotSnapshot(slotIndex, &clientIpAddress, NULL);
  }

  APP_HomeProtocol_InitParser(&parser);
  memset(&frame, 0, sizeof(frame));

  for (;;)
  {
    uint8_t buffer[APP_WIFI_LWIP_CLIENT_RX_BUFFER_SIZE];
    int received = 0;

    if (APP_WiFi_LwIP_IsCloseRequested(slotIndex) != 0U)
    {
      APP_WiFi_LwIP_LogClientEndpoint("client close requested",
                                      slotIndex,
                                      clientNode,
                                      clientIpAddress,
                                      clientPort,
                                      0,
                                      NULL);
      abortiveClose = 1U;
      break;
    }

    APP_WiFi_LwIP_DrainPendingControls(clientSocket, clientNode);

    received = recv(clientSocket, buffer, (int)sizeof(buffer), 0);
    if (received < 0)
    {
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
      {
#if (APP_WIFI_LWIP_CLIENT_IDLE_TIMEOUT_MS > 0U)
        const TickType_t nowTick = xTaskGetTickCount();

        if ((int32_t)(nowTick - lastRxTick) >= (int32_t)pdMS_TO_TICKS(APP_WIFI_LWIP_CLIENT_IDLE_TIMEOUT_MS))
        {
          printf("[lwip] client idle timeout node=%u\n", (unsigned int)clientNode);
          break;
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(APP_WIFI_LWIP_CONTROL_POLL_MS));
        continue;
      }

      APP_WiFi_LwIP_LogClientEndpoint("recv failed",
                                      slotIndex,
                                      clientNode,
                                      clientIpAddress,
                                      clientPort,
                                      (int32_t)errno,
                                      "errno");
      break;
    }

    if (received == 0)
    {
      APP_WiFi_LwIP_LogClientEndpoint("peer closed",
                                      slotIndex,
                                      clientNode,
                                      clientIpAddress,
                                      clientPort,
                                      0,
                                      NULL);
      break;
    }

#if (APP_WIFI_LWIP_CLIENT_IDLE_TIMEOUT_MS > 0U)
    lastRxTick = xTaskGetTickCount();
#endif

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
        APP_WiFi_LwIP_HandleProtocolFrame(clientSocket, slotIndex, &frame, &clientNode);
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

#if (APP_WIFI_LWIP_CLIENT_RX_YIELD_MS > 0U)
    vTaskDelay(pdMS_TO_TICKS(APP_WIFI_LWIP_CLIENT_RX_YIELD_MS));
#endif
  }

  if ((APP_WiFi_LwIP_IsKnownNode(clientNode) != 0U) &&
      (APP_WiFi_LwIP_HasOtherActiveNodeClient(slotIndex, clientNode) == 0U))
  {
    (void)APP_HomeData_UpdateNodeOnline(clientNode, 0U);
  }
  APP_WiFi_LwIP_RequeueInflightControl(clientNode);
  (void)APP_WiFi_LwIP_GetSlotSnapshot(slotIndex, &clientIpAddress, &clientNode);
  APP_WiFi_LwIP_ReleaseClientSlot(slotIndex);

  if (abortiveClose != 0U)
  {
    APP_WiFi_LwIP_AbortSocket(clientSocket);
  }
  else
  {
    closesocket(clientSocket);
  }
  APP_WiFi_LwIP_LogClientEndpoint("client disconnected",
                                  slotIndex,
                                  clientNode,
                                  clientIpAddress,
                                  clientPort,
                                  0,
                                  NULL);
  vTaskDelete(NULL);
}

/* Listening server thread accepting node clients and spawning per-client workers. */
static void APP_WiFi_LwIP_ServerTask(void *argument)
{
  (void)argument;

  /*
   * Long-running acceptor:
   * - wait until network stack is online
   * - open non-blocking listen socket
   * - accept clients and spawn dedicated client tasks
   * - rebuild server socket when network drops or repeated accept errors occur
   */
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

    status = listen(listenSocket, (int)APP_WIFI_LWIP_LISTEN_BACKLOG);
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

    {
      uint8_t acceptErrorCount = 0U;
      for (;;)
      {
      struct sockaddr_in clientAddress;
      socklen_t clientLength = (socklen_t)sizeof(clientAddress);
      int clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &clientLength);

      if (g_serverRebuildRequested != 0U)
      {
        g_serverRebuildRequested = 0U;
        printf("[lwip] rebuild server socket requested\n");
        break;
      }

      if (clientSocket < 0)
      {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
        {
          acceptErrorCount = 0U;
          if (APP_WiFi_LwIP_IsNetworkOnline() == 0U)
          {
            printf("[lwip] network offline, rebuild server socket\n");
            break;
          }

          vTaskDelay(pdMS_TO_TICKS(APP_WIFI_LWIP_ACCEPT_POLL_MS));
          continue;
        }

        printf("[lwip] accept failed errno=%d\n", errno);
        acceptErrorCount++;
        if (acceptErrorCount >= 3U)
        {
          printf("[lwip] repeated accept errors, rebuild server socket\n");
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(200U));
        continue;
      }
      acceptErrorCount = 0U;

      {
        const uint32_t clientIp = ntohl(clientAddress.sin_addr.s_addr);
        APP_WiFi_LwIP_ClientContext_t *clientContext = NULL;
        int32_t slotIndex = -1;
        int noDelay = 1;
        int keepAlive = 1;
        unsigned long clientNonBlocking = 1UL;
        struct timeval recvTimeout = {0};
        struct timeval sendTimeout = {0};

        recvTimeout.tv_sec = 0;
        recvTimeout.tv_usec = (int32_t)APP_WIFI_LWIP_CLIENT_RECV_TIMEOUT_MS * 1000;
        sendTimeout.tv_sec = 0;
        sendTimeout.tv_usec = (int32_t)APP_WIFI_LWIP_CLIENT_SEND_TIMEOUT_MS * 1000;

        (void)setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &noDelay, (socklen_t)sizeof(noDelay));
        (void)setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, (socklen_t)sizeof(keepAlive));
        (void)setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, (socklen_t)sizeof(recvTimeout));
        (void)setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, (socklen_t)sizeof(sendTimeout));
        if (ioctl(clientSocket, FIONBIO, &clientNonBlocking) != 0)
        {
          APP_WiFi_LwIP_LogClientEndpoint("client nonblocking failed",
                                          0xFFU,
                                          APP_HOME_NODE_NONE,
                                          clientIp,
                                          (uint16_t)ntohs(clientAddress.sin_port),
                                          0,
                                          NULL);
          closesocket(clientSocket);
          continue;
        }

        slotIndex = APP_WiFi_LwIP_ClaimClientSlot(clientIp, clientSocket);
        if (slotIndex < 0)
        {
          APP_WiFi_LwIP_LogClientEndpoint("no free slot, drop",
                                          0xFFU,
                                          APP_HOME_NODE_NONE,
                                          clientIp,
                                          (uint16_t)ntohs(clientAddress.sin_port),
                                          0,
                                          NULL);
          closesocket(clientSocket);
          continue;
        }

        APP_WiFi_LwIP_LogClientEndpoint("client connected",
                                        (uint8_t)slotIndex,
                                        APP_HOME_NODE_NONE,
                                        clientIp,
                                        (uint16_t)ntohs(clientAddress.sin_port),
                                        clientSocket,
                                        "socket");

        clientContext = (APP_WiFi_LwIP_ClientContext_t *)pvPortMalloc(sizeof(APP_WiFi_LwIP_ClientContext_t));
        if (clientContext == NULL)
        {
          printf("[lwip] client context alloc failed free_heap=%lu\n",
                 (unsigned long)xPortGetFreeHeapSize());
          APP_WiFi_LwIP_LogClientEndpoint("context alloc failed",
                                          (uint8_t)slotIndex,
                                          APP_HOME_NODE_NONE,
                                          clientIp,
                                          (uint16_t)ntohs(clientAddress.sin_port),
                                          clientSocket,
                                          "socket");
          APP_WiFi_LwIP_ReleaseClientSlot((uint8_t)slotIndex);
          closesocket(clientSocket);
          continue;
        }

        clientContext->socketHandle = clientSocket;
        clientContext->slotIndex = (uint8_t)slotIndex;
        clientContext->ipAddress = clientIp;
        clientContext->port = (uint16_t)ntohs(clientAddress.sin_port);
        if (sys_thread_new("wifi_cli",
                           APP_WiFi_LwIP_ClientTask,
                           clientContext,
                           APP_WIFI_LWIP_CLIENT_STACK_BYTES,
                           (int)APP_WIFI_LWIP_SOCKET_PRIORITY) == NULL)
        {
          printf("[lwip] start client thread failed free_heap=%lu\n",
                 (unsigned long)xPortGetFreeHeapSize());
          APP_WiFi_LwIP_LogClientEndpoint("start client thread failed",
                                          (uint8_t)slotIndex,
                                          APP_HOME_NODE_NONE,
                                          clientIp,
                                          (uint16_t)ntohs(clientAddress.sin_port),
                                          clientSocket,
                                          "socket");
          vPortFree(clientContext);
          APP_WiFi_LwIP_ReleaseClientSlot((uint8_t)slotIndex);
          closesocket(clientSocket);
          continue;
        }
      }
      }
    }

    closesocket(listenSocket);
    vTaskDelay(pdMS_TO_TICKS(1000U));
  }
}

/* One-time lwIP/tcpip/netif/server bootstrap and lazy resource allocation entrypoint. */
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
    APP_WiFi_LwIP_RegisterQueueForDebug((QueueHandle_t)g_tcpipInitSemaphore, "lwip_tcpip_init_sem");

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
    APP_WiFi_LwIP_RegisterQueueForDebug((QueueHandle_t)g_txQueueMutex, "lwip_tx_queue_mutex");
  }

  if (g_txFlushMutex == NULL)
  {
    g_txFlushMutex = xSemaphoreCreateMutex();
    if (g_txFlushMutex == NULL)
    {
      printf("[lwip] tx flush mutex allocation failed\n");
      return;
    }
    APP_WiFi_LwIP_RegisterQueueForDebug((QueueHandle_t)g_txFlushMutex, "lwip_tx_flush_mutex");
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

/* Inject one received Ethernet frame from AP6181 into lwIP input path. */
void APP_WiFi_LwIP_ProcessEthernetFrame(const uint8_t *frame, uint16_t length)
{
  struct pbuf *packet = NULL;
  err_t result = ERR_OK;

  if ((frame == NULL) || (length == 0U))
  {
    return;
  }

  APP_WiFi_LwIP_CachePeerFromFrame(frame, length);
  APP_WiFi_LwIP_LogRxFrame(frame, length);

  APP_WiFi_LwIP_EnsureInitialized();
  if ((g_netifAdded == 0U) || (g_netifUp == 0U))
  {
    return;
  }

  packet = pbuf_alloc(PBUF_RAW, length, PBUF_POOL_RX);
  if (packet == NULL)
  {
    g_rxPbufAllocFailCount++;
    if ((g_rxPbufAllocFailCount <= 8U) || ((g_rxPbufAllocFailCount % 64U) == 0U))
    {
      printf("[lwip] pbuf alloc failed #%lu len=%u\n",
             (unsigned long)g_rxPbufAllocFailCount,
             (unsigned int)length);
    }
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
    g_rxTcpipInputFailCount++;
    if ((g_rxTcpipInputFailCount <= 8U) || ((g_rxTcpipInputFailCount % 64U) == 0U))
    {
      printf("[lwip] tcpip_input failed #%lu err=%d len=%u\n",
             (unsigned long)g_rxTcpipInputFailCount,
             (int)result,
             (unsigned int)length);
    }
    pbuf_free(packet);
    return;
  }

  g_rxEthernetFrameCount++;

  APP_WiFi_LwIP_FlushTxQueue();
}

/* Periodic service hook for DHCP progression, ARP maintenance, and queue flush. */
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
        memset(g_peerCache, 0, sizeof(g_peerCache));
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
      char maskBuffer[16] = {0};
      char gwBuffer[16] = {0};

      if (ip4addr_ntoa_r(netif_ip4_addr(&g_wifiNetif), ipBuffer, sizeof(ipBuffer)) != NULL)
      {
        (void)ip4addr_ntoa_r(netif_ip4_netmask(&g_wifiNetif), maskBuffer, sizeof(maskBuffer));
        (void)ip4addr_ntoa_r(netif_ip4_gw(&g_wifiNetif), gwBuffer, sizeof(gwBuffer));
        printf("[lwip] dhcp bound ip=%s mask=%s gw=%s mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
               ipBuffer,
               maskBuffer,
               gwBuffer,
               (unsigned int)g_wifiNetif.hwaddr[0],
               (unsigned int)g_wifiNetif.hwaddr[1],
               (unsigned int)g_wifiNetif.hwaddr[2],
               (unsigned int)g_wifiNetif.hwaddr[3],
               (unsigned int)g_wifiNetif.hwaddr[4],
               (unsigned int)g_wifiNetif.hwaddr[5]);
      }
      else
      {
        printf("[lwip] dhcp bound\n");
      }

      g_ipAnnounced = 1U;
      g_nextArpRefreshTick = 0U;
      g_arpRefreshCount = 0U;
    }

    APP_WiFi_LwIP_DrainAllPendingControls();
    APP_WiFi_LwIP_FlushTxQueue();
#if (APP_WIFI_LWIP_GRATUITOUS_ARP_ENABLE != 0U)
    APP_WiFi_LwIP_MaybeRefreshArp();
    APP_WiFi_LwIP_FlushTxQueue();
#endif
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
      memset(g_peerCache, 0, sizeof(g_peerCache));
      APP_WiFi_LwIP_RequestCloseAllClients();
      APP_WiFi_LwIP_ResetControlState();
      APP_WiFi_LwIP_DropQueuedTx();
      printf("[lwip] netif down\n");
    }
  }
}
