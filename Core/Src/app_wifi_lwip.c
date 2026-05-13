#include "app_wifi_lwip.h"

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
#define APP_WIFI_LWIP_TX_QUEUE_DEPTH        8U
#define APP_WIFI_LWIP_SOCKET_STACK_BYTES    4096U
#define APP_WIFI_LWIP_SOCKET_PRIORITY       3U

typedef struct
{
  uint8_t *data;
  uint16_t length;
} APP_WiFi_LwIP_TxPacket_t;

static struct netif g_wifiNetif = {0};
static SemaphoreHandle_t g_tcpipInitSemaphore = NULL;
static SemaphoreHandle_t g_txQueueMutex = NULL;
static APP_WiFi_LwIP_TxPacket_t g_txQueue[APP_WIFI_LWIP_TX_QUEUE_DEPTH] = {0};
static uint8_t g_txQueueHead = 0U;
static uint8_t g_txQueueTail = 0U;
static uint8_t g_txQueueCount = 0U;
static uint8_t g_tcpipStarted = 0U;
static uint8_t g_netifAdded = 0U;
static uint8_t g_netifUp = 0U;
static uint8_t g_netifMacSynced = 0U;
static uint8_t g_dhcpStarted = 0U;
static uint8_t g_ipAnnounced = 0U;
static uint8_t g_serverStarted = 0U;
static uint8_t g_lwipReadyLogged = 0U;
static uint32_t g_linkOutputCount = 0U;
static uint32_t g_txOkCount = 0U;
static uint32_t g_txBusyCount = 0U;
static uint32_t g_txErrorCount = 0U;
static uint32_t g_serverRxCount = 0U;
static uint32_t g_serverFrameCount = 0U;
static uint32_t g_serverAckCount = 0U;
static uint32_t g_serverErrCount = 0U;
static uint32_t g_serverSendErrorCount = 0U;

static void APP_WiFi_LwIP_TcpipInitDone(void *arg);
static err_t APP_WiFi_LwIP_NetifInit(struct netif *netif);
static err_t APP_WiFi_LwIP_LinkOutput(struct netif *netif, struct pbuf *p);
static uint8_t APP_WiFi_LwIP_QueuePush(uint8_t *data, uint16_t length);
static uint8_t APP_WiFi_LwIP_QueuePeek(APP_WiFi_LwIP_TxPacket_t *packet);
static void APP_WiFi_LwIP_QueuePop(void);
static void APP_WiFi_LwIP_DropQueuedTx(void);
static void APP_WiFi_LwIP_FlushTxQueue(void);
static void APP_WiFi_LwIP_StartServerTask(void);
static void APP_WiFi_LwIP_ServerTask(void *argument);
static void APP_WiFi_LwIP_EnsureInitialized(void);
static err_t APP_WiFi_LwIP_RefreshNetifMacAddress(struct netif *netif);
static uint8_t APP_WiFi_LwIP_SendAll(int socketHandle, const uint8_t *data, uint16_t length);
static void APP_WiFi_LwIP_HandleProtocolFrame(int clientSocket, const APP_HomeProtocolFrame_t *frame);
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
  if (g_linkOutputCount <= 8U)
  {
    printf("[lwip] linkoutput #%lu len=%u type=0x%02X%02X q=%u\n",
           (unsigned long)g_linkOutputCount,
           (unsigned int)packetLength,
           (packetLength >= 13U) ? (unsigned int)copy[12] : 0U,
           (packetLength >= 14U) ? (unsigned int)copy[13] : 0U,
           (unsigned int)g_txQueueCount);
  }

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

  for (;;)
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
      if (g_txOkCount <= 8U)
      {
        printf("[lwip] tx ok #%lu len=%u\n",
               (unsigned long)g_txOkCount,
               (unsigned int)packet.length);
      }

      APP_WiFi_LwIP_QueuePop();
      if (packet.data != NULL)
      {
        vPortFree(packet.data);
      }
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
      if (g_txBusyCount <= 8U)
      {
        printf("[lwip] tx busy #%lu len=%u\n",
               (unsigned long)g_txBusyCount,
               (unsigned int)packet.length);
      }
    }

    break;
  }
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

static void APP_WiFi_LwIP_HandleProtocolFrame(int clientSocket, const APP_HomeProtocolFrame_t *frame)
{
  uint8_t shouldAck = 1U;
  APP_HomeProtocolError_t error = APP_HOME_ERR_NONE;

  if (frame == NULL)
  {
    return;
  }

  g_serverFrameCount++;
  printf("[proto] frame #%lu node=%u cmd=%s(0x%02X) seq=%u len=%u\n",
         (unsigned long)g_serverFrameCount,
         (unsigned int)frame->node,
         APP_HomeProtocol_CommandToString(frame->command),
         (unsigned int)frame->command,
         (unsigned int)frame->sequence,
         (unsigned int)frame->length);

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
                                           frame->payload[6]);
        printf("[proto] telemetry node=%u temp_x10=%d hum_x10=%u mode=%u fan=%u online=%u\n",
               (unsigned int)frame->node,
               (int)temperature,
               (unsigned int)humidity,
               (unsigned int)frame->payload[4],
               (unsigned int)frame->payload[5],
               (unsigned int)frame->payload[6]);
      }
      break;

    case APP_HOME_CMD_CONTROL:
      if (APP_WiFi_LwIP_IsKnownNode(frame->node) == 0U)
      {
        error = APP_HOME_ERR_BAD_NODE;
      }
      else if (frame->length != 5U)
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
        printf("[proto] status node=%u code=%u\n",
               (unsigned int)frame->node,
               (unsigned int)frame->payload[0]);
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
        printf("[proto] heartbeat node=%u\n", (unsigned int)frame->node);
      }
      break;

    case APP_HOME_CMD_ACK:
      shouldAck = 0U;
      printf("[proto] peer ack node=%u seq=%u\n",
             (unsigned int)frame->node,
             (unsigned int)frame->sequence);
      break;

    case APP_HOME_CMD_ERR:
      shouldAck = 0U;
      printf("[proto] peer err node=%u seq=%u\n",
             (unsigned int)frame->node,
             (unsigned int)frame->sequence);
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

static void APP_WiFi_LwIP_ServerTask(void *argument)
{
  (void)argument;

  for (;;)
  {
    int listenSocket = -1;
    struct sockaddr_in serverAddress;
    int reuse = 1;
    int status = 0;

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

    status = listen(listenSocket, 1);
    if (status != 0)
    {
      printf("[lwip] listen failed\n");
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
        printf("[lwip] accept failed\n");
        break;
      }

      printf("[lwip] client connected\n");

      {
        APP_HomeProtocolParser_t parser;
        APP_HomeProtocolFrame_t frame;

        APP_HomeProtocol_InitParser(&parser);
        memset(&frame, 0, sizeof(frame));

        for (;;)
        {
          uint8_t buffer[256];
          int received = recv(clientSocket, buffer, (int)sizeof(buffer), 0);

          if (received <= 0)
          {
            break;
          }

          g_serverRxCount++;
          if (g_serverRxCount <= 16U)
          {
            printf("[lwip] recv #%lu len=%d\n",
                   (unsigned long)g_serverRxCount,
                   received);
          }

          for (int index = 0; index < received; index++)
          {
            APP_HomeProtocolParseResult_t parseResult =
              APP_HomeProtocol_PushByte(&parser, buffer[index], &frame);

            if (parseResult == APP_HOME_PARSE_FRAME)
            {
              APP_WiFi_LwIP_HandleProtocolFrame(clientSocket, &frame);
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
      }

      closesocket(clientSocket);
      printf("[lwip] client disconnected\n");
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
  }
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
        printf("[lwip] netif up, starting DHCP\n");
      }
      else
      {
        return;
      }
    }

    if (g_dhcpStarted == 0U)
    {
      if (netifapi_dhcp_start(&g_wifiNetif) == ERR_OK)
      {
        g_dhcpStarted = 1U;
        printf("[lwip] dhcp start\n");
      }
    }

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
    }

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
      APP_WiFi_LwIP_DropQueuedTx();
      printf("[lwip] netif down\n");
    }
  }
}
