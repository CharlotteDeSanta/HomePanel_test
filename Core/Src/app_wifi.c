#include "app_wifi.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "app_debug_uart.h"
#include "app_wifi_lwip.h"
#include "app_wifi_resources.h"
#include "app_wifi_platform.h"
#include "main.h"

#define APP_WIFI_STACK_WAIT_MS          6U
#define APP_WIFI_RESET_ASSERT_MS        20U
#define APP_WIFI_RESET_RELEASE_GUARD_MS 50U
#define APP_WIFI_MODULE_SETTLE_MS       200U
#define APP_WIFI_POLL_MS                1000U
#define APP_WIFI_RX_DRAIN_BURST_MAIN    12U
#define APP_WIFI_RX_DRAIN_BURST_EXTRA   8U
#define APP_WIFI_RX_DRAIN_EXTRA_ROUNDS  1U
#define APP_WIFI_HEARTBEAT_MS           5000U
#define APP_WIFI_HEARTBEAT_LOG_ENABLE   0U
#define APP_WIFI_SDPCM_TRACE_ENABLE     0U
#define APP_WIFI_DATA_TX_LOG_ENABLE     0U
#define APP_WIFI_SCAN_ACTIVE_POLL_MS    100U
#define APP_WIFI_SCAN_TIMEOUT_MS        15000U
#define APP_WIFI_SCAN_STEP_TIMEOUT_MS   3000U
#define APP_WIFI_LOG_BUFFER_SIZE        1024U
#define APP_WIFI_SDIO_FN2               2U
#define APP_WIFI_SDPCM_FRAME_AVAILABLE_MASK 0x000000F0U
#define APP_WIFI_SDPCM_HW_TAG_SIZE      4U
#define APP_WIFI_SDPCM_HEADER_SIZE      12U
#define APP_WIFI_SDPCM_DATA_PADDING_SIZE 2U
#define APP_WIFI_SDPCM_CDC_HEADER_SIZE  16U
#define APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE 256U
#define APP_WIFI_SDPCM_MAX_FRAME_LEN    2048U
#define APP_WIFI_SDPCM_CHANNEL_MASK     0x0FU
#define APP_WIFI_SDPCM_CHANNEL_CONTROL  0U
#define APP_WIFI_SDPCM_CHANNEL_EVENT    1U
#define APP_WIFI_SDPCM_CHANNEL_DATA     2U
#define APP_WIFI_SDPCM_BDC_HEADER_SIZE  4U
#define APP_WIFI_SDPCM_FN2_BLOCK_SIZE   64U
#define APP_WIFI_BDC_PROTO_VER          2U
#define APP_WIFI_BDC_FLAG_VER_SHIFT     4U
#define APP_WIFI_ETHERNET_MIN_FRAME_LEN 60U
#define APP_WIFI_SDPCM_IOCTL_GET        0x00U
#define APP_WIFI_SDPCM_IOCTL_SET        0x02U
#define APP_WIFI_SDPCM_IOCTL_ID_SHIFT   16U
#define APP_WIFI_SDPCM_IOCTL_ID_MASK    0xFFFF0000UL
#define APP_WIFI_SDPCM_IOCTL_IF_SHIFT   12U
#define APP_WIFI_IOCTL_U32_SIZE         4U
#define APP_WIFI_BRCM_ETHER_TYPE        0x886CU
#define APP_WIFI_BRCM_EVENT_OUI_LEN     3U
#define APP_WIFI_WLC_GET_VERSION        1UL
#define APP_WIFI_WLC_UP                 2UL
#define APP_WIFI_WLC_SET_INFRA          20UL
#define APP_WIFI_WLC_SET_AUTH           22UL
#define APP_WIFI_WLC_GET_BSSID          23UL
#define APP_WIFI_WLC_SET_SSID           26UL
#define APP_WIFI_WLC_E_SCAN_COMPLETE    26UL
#define APP_WIFI_WLC_E_SET_SSID         0UL
#define APP_WIFI_WLC_E_JOIN             1UL
#define APP_WIFI_WLC_E_AUTH             3UL
#define APP_WIFI_WLC_E_DEAUTH_IND       6UL
#define APP_WIFI_WLC_E_ASSOC            7UL
#define APP_WIFI_WLC_E_DISASSOC_IND     12UL
#define APP_WIFI_WLC_E_LINK             16UL
#define APP_WIFI_WLC_E_PRUNE            23UL
#define APP_WIFI_WLC_E_PSK_SUP          46UL
#define APP_WIFI_WLC_E_ESCAN_RESULT     69UL
#define APP_WIFI_WLC_SET_PM             86UL
#define APP_WIFI_WLC_SET_GMODE          110UL
#define APP_WIFI_WLC_SET_SCANSUPPRESS   116UL
#define APP_WIFI_WLC_SET_WSEC           134UL
#define APP_WIFI_WLC_SET_WPA_AUTH       165UL
#define APP_WIFI_WLC_GET_VAR            262UL
#define APP_WIFI_WLC_SET_VAR            263UL
#define APP_WIFI_WLC_SET_WSEC_PMK       268UL
#define APP_WIFI_IOVAR_CUR_ETHERADDR    "cur_etheraddr"
#define APP_WIFI_IOVAR_TX_GLOM          "bus:txglom"
#define APP_WIFI_IOVAR_APSTA            "apsta"
#define APP_WIFI_IOVAR_COUNTRY          "country"
#define APP_WIFI_IOVAR_EVENT_MSGS       "event_msgs"
#define APP_WIFI_IOVAR_BSSCFG_EVENT_MSGS "bsscfg:event_msgs"
#define APP_WIFI_IOVAR_BSSCFG_SUP_WPA   "bsscfg:sup_wpa"
#define APP_WIFI_IOVAR_BSSCFG_SUP_WPA2_EAPVER "bsscfg:sup_wpa2_eapver"
#define APP_WIFI_IOVAR_BSSCFG_SUP_WPA_TMO "bsscfg:sup_wpa_tmo"
#define APP_WIFI_IOVAR_ESCAN            "escan"
#define APP_WIFI_PM_OFF                 0UL
#define APP_WIFI_WL_EVENTING_MASK_LEN   19U
#define APP_WIFI_STA_BSSCFG_INDEX       0UL
#define APP_WIFI_WL_BSS_INFO_VERSION    109UL
#define APP_WIFI_WL_BSSTYPE_ANY         2
#define APP_WIFI_WL_SCAN_ACTION_START   1U
#define APP_WIFI_ESCAN_REQ_VERSION      1UL
#define APP_WIFI_ESCAN_SYNC_ID          0U
#define APP_WIFI_GMODE_AUTO             1UL
#define APP_WIFI_APSTA_ENABLE           1UL
#define APP_WIFI_COUNTRY_CODE           "CN"
#define APP_WIFI_COUNTRY_REV            (-1)
#define APP_WIFI_JOIN_MAX_SSID_LEN      32U
#define APP_WIFI_JOIN_MAX_PASSWORD_LEN  64U
#define APP_WIFI_WSEC_MIN_PSK_LEN       8U
#define APP_WIFI_WSEC_MAX_PSK_LEN       64U
#define APP_WIFI_WSEC_PASSPHRASE        0x0001U
#define APP_WIFI_WSEC_AES               0x0004UL
#define APP_WIFI_EAPOL_KEY_TIMEOUT_MS   2500L
#define APP_WIFI_LOCAL_STATUS_TIMEOUT   0xFFFFFFFEUL
#define APP_WIFI_BCME_UNSUPPORTED       ((uint32_t)(int32_t)-23)
#define APP_WIFI_WPA_AUTH_DISABLED      0x0000UL
#define APP_WIFI_WPA2_AUTH_PSK          0x0080UL
#define APP_WIFI_WL_AUTH_OPEN_SYSTEM    0UL
#define APP_WIFI_WL_AUTH_SHARED_KEY     1UL
#define APP_WIFI_WLC_EVENT_MSG_LINK     0x0001UL
#define APP_WIFI_JOIN_LINK_POLL_INTERVAL_MS 1000U
#define APP_WIFI_JOIN_LINK_TIMEOUT_MS   15000U
#define APP_WIFI_JOIN_RETRY_DELAY_MS    5000U
#define APP_WIFI_JOIN_RETRY_MAX         4U
#define APP_WIFI_CONNECTED_LINK_DOWN_GRACE_MS 15000U
#define APP_WIFI_CREDIT_STALL_MIN_HITS  8U
#define APP_WIFI_CREDIT_STALL_WINDOW_MS 1200U
#define APP_WIFI_CREDIT_STALL_GAP_MS    250U
#define APP_WIFI_RECOVERY_JOIN_DELAY_MS 1200U
#define APP_WIFI_WLC_E_STATUS_SUCCESS   0UL
#define APP_WIFI_WLC_E_STATUS_NO_NETWORKS 3UL
#define APP_WIFI_WLC_E_STATUS_ABORT     4UL
#define APP_WIFI_WLC_E_STATUS_UNSOLICITED 6UL
#define APP_WIFI_WLC_E_STATUS_PARTIAL   8UL
#define APP_WIFI_WLC_E_STATUS_NEWSCAN   9UL
#define APP_WIFI_WLC_E_STATUS_NEWASSOC  10UL
#define APP_WIFI_WLC_SUP_KEYED          6UL
#define APP_WIFI_WLC_SUP_KEYXCHANGE_WAIT_M1 4UL
#define APP_WIFI_WLC_SUP_KEYXCHANGE_WAIT_M3 8UL
#define APP_WIFI_WLC_SUP_KEYXCHANGE_WAIT_G1 10UL
#define APP_WIFI_WLC_E_SUP_WPA_PSK_TMO  15UL

typedef struct
{
  uint32_t SSID_len;
  uint8_t SSID[32];
} APP_WiFi_Ssid_t;

typedef struct
{
  uint8_t octet[APP_WIFI_MAC_ADDRESS_SIZE];
} APP_WiFi_EtherAddr_t;

typedef struct
{
  APP_WiFi_Ssid_t ssid;
  APP_WiFi_EtherAddr_t bssid;
  int8_t bss_type;
  int8_t scan_type;
  int32_t nprobes;
  int32_t active_time;
  int32_t passive_time;
  int32_t home_time;
  int32_t channel_num;
  uint16_t channel_list[1];
} APP_WiFi_ScanParams_t;

typedef struct
{
  uint32_t version;
  uint16_t action;
  uint16_t sync_id;
  APP_WiFi_ScanParams_t params;
} APP_WiFi_EscanParams_t;

typedef struct
{
  uint32_t buflen;
  uint32_t version;
  uint16_t sync_id;
  uint16_t bss_count;
} APP_WiFi_EscanResult_t;

typedef struct
{
  char country_abbrev[4];
  int32_t rev;
  char ccode[4];
} APP_WiFi_Country_t;

typedef struct
{
  uint16_t key_len;
  uint16_t flags;
  uint8_t key[APP_WIFI_WSEC_MAX_PSK_LEN];
} APP_WiFi_Pmk_t;

typedef enum
{
  APP_WIFI_IOVAR_REQUEST_NONE = 0,
  APP_WIFI_IOVAR_REQUEST_CUR_ETHERADDR_GET,
  APP_WIFI_IOVAR_REQUEST_TX_GLOM_SET,
  APP_WIFI_IOVAR_REQUEST_APSTA_SET,
  APP_WIFI_IOVAR_REQUEST_COUNTRY_SET,
  APP_WIFI_IOVAR_REQUEST_EVENT_MSGS_SET,
  APP_WIFI_IOVAR_REQUEST_SUP_WPA_SET,
  APP_WIFI_IOVAR_REQUEST_SUP_WPA2_EAPVER_SET,
  APP_WIFI_IOVAR_REQUEST_SUP_WPA_TMO_SET,
  APP_WIFI_IOVAR_REQUEST_ESCAN_SET
} APP_WiFi_IovarRequest_t;

typedef enum
{
  APP_WIFI_JOIN_SECURITY_OPEN = 0,
  APP_WIFI_JOIN_SECURITY_WPA2_PSK
} APP_WiFi_JoinSecurity_t;

typedef enum
{
  APP_WIFI_JOIN_STEP_IDLE = 0,
  APP_WIFI_JOIN_STEP_SEND_WSEC,
  APP_WIFI_JOIN_STEP_WAIT_WSEC,
  APP_WIFI_JOIN_STEP_SEND_SUP_WPA,
  APP_WIFI_JOIN_STEP_WAIT_SUP_WPA,
  APP_WIFI_JOIN_STEP_SEND_SUP_WPA2_EAPVER,
  APP_WIFI_JOIN_STEP_WAIT_SUP_WPA2_EAPVER,
  APP_WIFI_JOIN_STEP_SEND_SUP_WPA_TMO,
  APP_WIFI_JOIN_STEP_WAIT_SUP_WPA_TMO,
  APP_WIFI_JOIN_STEP_SEND_PMK,
  APP_WIFI_JOIN_STEP_WAIT_PMK,
  APP_WIFI_JOIN_STEP_SEND_INFRA,
  APP_WIFI_JOIN_STEP_WAIT_INFRA,
  APP_WIFI_JOIN_STEP_SEND_AUTH,
  APP_WIFI_JOIN_STEP_WAIT_AUTH,
  APP_WIFI_JOIN_STEP_SEND_WPA_AUTH,
  APP_WIFI_JOIN_STEP_WAIT_WPA_AUTH,
  APP_WIFI_JOIN_STEP_SEND_SSID,
  APP_WIFI_JOIN_STEP_WAIT_SSID_ACK,
  APP_WIFI_JOIN_STEP_WAIT_EVENTS
} APP_WiFi_JoinStep_t;

typedef enum
{
  APP_WIFI_SCAN_STEP_IDLE = 0,
  APP_WIFI_SCAN_STEP_SEND_EVENT_MASK,
  APP_WIFI_SCAN_STEP_WAIT_EVENT_MASK,
  APP_WIFI_SCAN_STEP_SEND_SCAN_SUPPRESS,
  APP_WIFI_SCAN_STEP_WAIT_SCAN_SUPPRESS,
  APP_WIFI_SCAN_STEP_SEND_PM_OFF,
  APP_WIFI_SCAN_STEP_WAIT_PM_OFF,
  APP_WIFI_SCAN_STEP_SEND_ESCAN,
  APP_WIFI_SCAN_STEP_WAIT_ESCAN_ACK,
  APP_WIFI_SCAN_STEP_WAIT_RESULTS
} APP_WiFi_ScanStep_t;

typedef struct
{
  uint32_t version;
  uint32_t length;
  APP_WiFi_EtherAddr_t bssid;
  uint16_t beacon_period;
  uint16_t capability;
  uint8_t ssid_length;
  uint8_t ssid[32];
  struct
  {
    uint32_t count;
    uint8_t rates[16];
  } rateset;
  uint16_t chanspec;
  uint16_t atim_window;
  uint8_t dtim_period;
  int16_t rssi;
  int8_t phy_noise;
  uint8_t n_cap;
  uint32_t nbss_cap;
  uint8_t ctl_ch;
  uint32_t reserved32[1];
  uint8_t flags;
  uint8_t reserved[3];
  uint8_t basic_mcs[16];
  uint16_t ie_offset;
  uint32_t ie_length;
  int16_t snr;
} APP_WiFi_BssInfo_t;

#pragma pack(push, 1)
typedef struct
{
  uint8_t flags;
  uint8_t priority;
  uint8_t flags2;
  uint8_t data_offset;
} APP_WiFi_BdcHeader_t;

typedef struct
{
  uint8_t destination_address[APP_WIFI_MAC_ADDRESS_SIZE];
  uint8_t source_address[APP_WIFI_MAC_ADDRESS_SIZE];
  uint16_t ethertype;
} APP_WiFi_EthernetHeader_t;

typedef struct
{
  uint16_t subtype;
  uint16_t length;
  uint8_t version;
  uint8_t oui[APP_WIFI_BRCM_EVENT_OUI_LEN];
  uint16_t user_subtype;
} APP_WiFi_BcmEthHeader_t;

typedef struct
{
  uint16_t version;
  uint16_t flags;
  uint32_t event_type;
  uint32_t status;
  uint32_t reason;
  uint32_t auth_type;
  uint32_t datalen;
  uint8_t address[APP_WIFI_MAC_ADDRESS_SIZE];
  char ifname[16];
  uint8_t ifidx;
  uint8_t bss_cfg_idx;
} APP_WiFi_RawEventHeader_t;

typedef struct
{
  APP_WiFi_EthernetHeader_t ether;
  APP_WiFi_BcmEthHeader_t bcmeth;
  APP_WiFi_RawEventHeader_t raw;
} APP_WiFi_BcmEvent_t;
#pragma pack(pop)

static volatile APP_WiFiState_t g_wifiState = APP_WIFI_STATE_IDLE;
static volatile uint32_t g_wifiOobInterruptCount = 0U;
static uint32_t g_wifiLastHeartbeatTick = 0U;
static uint32_t g_wifiSdpcmFrameCount = 0U;
static uint32_t g_wifiLastSdpcmFrameInterrupt = 0U;
static uint16_t g_wifiLastSdpcmFrameLength = 0U;
static uint8_t g_wifiLastSdpcmSequence = 0U;
static uint8_t g_wifiLastSdpcmChannel = 0U;
static uint8_t g_wifiLastSdpcmHeaderLength = 0U;
static uint8_t g_wifiLastSdpcmNextLength = 0U;
static uint8_t g_wifiLastSdpcmBusCredit = 0U;
static uint8_t g_wifiLastSdpcmFlowControl = 0U;
static uint8_t g_wifiBusCredit = 1U;
static uint8_t g_wifiBusCreditDiff = 0U;
static uint8_t g_wifiTxSequence = 0U;
static uint16_t g_wifiIoctlRequestId = 0U;
static uint8_t g_wifiIoctlProbeSent = 0U;
static uint8_t g_wifiIoctlProbeCompleted = 0U;
static uint8_t g_wifiVersionProbeCompleted = 0U;
static uint8_t g_wifiUpProbeSent = 0U;
static uint8_t g_wifiUpProbeCompleted = 0U;
static uint8_t g_wifiTxGlomSent = 0U;
static uint8_t g_wifiTxGlomCompleted = 0U;
static uint8_t g_wifiApstaSent = 0U;
static uint8_t g_wifiApstaCompleted = 0U;
static uint8_t g_wifiCountrySent = 0U;
static uint8_t g_wifiCountryCompleted = 0U;
static uint8_t g_wifiGmodeSent = 0U;
static uint8_t g_wifiGmodeCompleted = 0U;
static uint8_t g_wifiMacProbeSent = 0U;
static uint8_t g_wifiMacProbeCompleted = 0U;
static uint8_t g_wifiEventMaskSent = 0U;
static uint8_t g_wifiEventMaskCompleted = 0U;
static uint8_t g_wifiScanSuppressSent = 0U;
static uint8_t g_wifiScanSuppressCompleted = 0U;
static uint8_t g_wifiPmProbeSent = 0U;
static uint8_t g_wifiPmProbeCompleted = 0U;
static uint8_t g_wifiScanSent = 0U;
static uint8_t g_wifiScanIoctlCompleted = 0U;
static uint8_t g_wifiScanCompleted = 0U;
static uint8_t g_wifiScanAborted = 0U;
static volatile uint8_t g_wifiScanRequested = 0U;
static APP_WiFi_ScanStep_t g_wifiScanStep = APP_WIFI_SCAN_STEP_IDLE;
static uint32_t g_wifiScanStepStartTick = 0U;
static uint32_t g_wifiScanStartTick = 0U;
static uint8_t g_wifiLastMacAddress[APP_WIFI_MAC_ADDRESS_SIZE] = {0U};
static uint8_t g_wifiLastBssid[APP_WIFI_MAC_ADDRESS_SIZE] = {0U};
static char g_wifiLastScanSsid[33] = {0};
static APP_WiFiScanResult_t g_wifiCachedScanResults[APP_WIFI_SCAN_RESULT_CACHE_SIZE] = {0};
static uint32_t g_wifiCachedScanResultCount = 0U;
static volatile APP_WiFiLinkState_t g_wifiLinkState = APP_WIFI_LINK_STATE_IDLE;
static APP_WiFi_JoinStep_t g_wifiJoinStep = APP_WIFI_JOIN_STEP_IDLE;
static APP_WiFi_JoinSecurity_t g_wifiJoinSecurity = APP_WIFI_JOIN_SECURITY_OPEN;
static char g_wifiJoinRequestedSsid[APP_WIFI_JOIN_MAX_SSID_LEN + 1U] = {0};
static uint8_t g_wifiJoinRequestedSsidLength = 0U;
static uint8_t g_wifiJoinRequestedPassphrase[APP_WIFI_JOIN_MAX_PASSWORD_LEN + 1U] = {0U};
static uint8_t g_wifiJoinRequestedPassphraseLength = 0U;
static uint8_t g_wifiJoinSsidSet = 0U;
static uint8_t g_wifiJoinAuthenticated = 0U;
static uint8_t g_wifiJoinLinkReady = 0U;
static uint8_t g_wifiJoinSecurityComplete = 0U;
static uint32_t g_wifiJoinFailureStatus = 0U;
static uint32_t g_wifiJoinFailureReason = 0U;
static uint32_t g_wifiLastJoinEventType = 0U;
static uint32_t g_wifiLastJoinEventStatus = 0U;
static uint32_t g_wifiLastJoinEventReason = 0U;
static uint32_t g_wifiJoinStartTick = 0U;
static uint32_t g_wifiJoinLastPollTick = 0U;
static uint32_t g_wifiJoinRetryAtTick = 0U;
static uint8_t g_wifiJoinRetryCount = 0U;
static uint8_t g_wifiConnectedLinkDownPending = 0U;
static uint32_t g_wifiConnectedLinkDownTick = 0U;
static uint32_t g_wifiConnectedLinkDownStatus = 0U;
static uint32_t g_wifiConnectedLinkDownReason = 0U;
static uint32_t g_wifiConnectedLinkDownFrameCount = 0U;
static uint32_t g_wifiScanPartialCount = 0U;
static uint32_t g_wifiScanResultCount = 0U;
static uint16_t g_wifiLastScanSyncId = 0U;
static uint16_t g_wifiLastScanBssCount = 0U;
static int16_t g_wifiLastScanRssi = 0;
static uint8_t g_wifiLastScanChannel = 0U;
static uint32_t g_wifiLastScanPollTick = 0U;
static uint32_t g_wifiLastIoctlCommand = 0U;
static uint32_t g_wifiLastIoctlStatus = 0U;
static uint32_t g_wifiLastIoctlFlags = 0U;
static uint32_t g_wifiLastIoctlValue = 0U;
static APP_WiFi_IovarRequest_t g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_NONE;
static uint8_t g_wifiRxFrameBuffer[APP_WIFI_SDPCM_MAX_FRAME_LEN] = {0U};
static uint8_t g_wifiNoCreditHitCount = 0U;
static uint8_t g_wifiNoCreditTxSequenceSnapshot = 0xFFU;
static uint8_t g_wifiNoCreditBusCreditSnapshot = 0xFFU;
static uint32_t g_wifiNoCreditFirstTick = 0U;
static uint32_t g_wifiNoCreditLastTick = 0U;

static const char *APP_WiFi_StateToString(APP_WiFiState_t state);
static const char *APP_WiFi_SdpcmChannelToString(uint8_t channel);
static void APP_WiFi_Logf(const char *format, ...);
static void APP_WiFi_LogStateDetails(APP_WiFiState_t state);
static void APP_WiFi_LogPeriodicHeartbeat(void);
static uint16_t APP_WiFi_ReadLe16(const uint8_t *bytes);
static uint32_t APP_WiFi_ReadLe32(const uint8_t *bytes);
static uint16_t APP_WiFi_ReadBe16(const uint8_t *bytes);
static uint32_t APP_WiFi_ReadBe32(const uint8_t *bytes);
static void APP_WiFi_WriteLe16(uint8_t *bytes, uint16_t value);
static void APP_WiFi_WriteLe32(uint8_t *bytes, uint32_t value);
static uint16_t APP_WiFi_GetSdpcmChunkSize(uint16_t remaining);
static void APP_WiFi_RecordSdpcmHeader(const uint8_t *header, uint16_t frameLength, uint32_t frameInterrupt);
static void APP_WiFi_UpdateSdpcmCredit(const uint8_t *header);
static void APP_WiFi_LogSdpcmBytes(const char *prefix, const uint8_t *data, uint16_t length);
static void APP_WiFi_LogMacAddress(const char *prefix, const uint8_t *macAddress);
static int APP_WiFi_ParseHexNibble(char character);
static uint8_t APP_WiFi_ParseNvramMacAddress(const char *text, uint8_t *macAddress);
static uint8_t APP_WiFi_IsMacAddressUsable(const uint8_t *macAddress);
static void APP_WiFi_GenerateFallbackMacAddress(uint8_t *macAddress);
static void APP_WiFi_SetEventMaskBit(uint8_t *eventMask, uint16_t eventNumber);
static void APP_WiFi_CopyScanSsid(char *destination, const uint8_t *ssid, uint8_t ssidLength);
static void APP_WiFi_ClearCachedScanResults(void);
static int32_t APP_WiFi_FindCachedScanResult(const uint8_t *bssid);
static void APP_WiFi_UpdateCachedScanResult(const APP_WiFi_BssInfo_t *bssInfo);
static HAL_StatusTypeDef APP_WiFi_SendBufferedControlIoctl(uint8_t ioctlType,
                                                           uint32_t command,
                                                           const void *payload,
                                                           uint16_t payloadLength,
                                                           const char *name);
static HAL_StatusTypeDef APP_WiFi_SendControlIoctl(uint8_t ioctlType,
                                                   uint32_t command,
                                                   const void *payload,
                                                   uint16_t payloadLength,
                                                   const char *name);
static HAL_StatusTypeDef APP_WiFi_SendGetVersionIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendUpIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendSetU32Iovar(const char *iovarName,
                                                  uint32_t value,
                                                  const char *logName,
                                                  APP_WiFi_IovarRequest_t requestType);
static HAL_StatusTypeDef APP_WiFi_SendDisableTxGlomIovar(void);
static HAL_StatusTypeDef APP_WiFi_SendEnableApstaIovar(void);
static HAL_StatusTypeDef APP_WiFi_SendSetCountryIovar(void);
static HAL_StatusTypeDef APP_WiFi_SendSetGmodeAutoIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendGetCurEtheraddrIovar(void);
static HAL_StatusTypeDef APP_WiFi_SendSetEventMsgsIovar(void);
static HAL_StatusTypeDef APP_WiFi_SendSetBsscfgU32Iovar(const char *iovarName,
                                                        int32_t value,
                                                        const char *logName,
                                                        APP_WiFi_IovarRequest_t requestType);
static HAL_StatusTypeDef APP_WiFi_SendSetWsecIoctl(uint32_t wsec);
static HAL_StatusTypeDef APP_WiFi_SendSetInfraIoctl(uint32_t infraMode);
static HAL_StatusTypeDef APP_WiFi_SendSetAuthIoctl(uint32_t authMode);
static HAL_StatusTypeDef APP_WiFi_SendSetWpaAuthIoctl(uint32_t wpaAuth);
static HAL_StatusTypeDef APP_WiFi_SendGetBssidIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendSetSupWpaIovar(uint32_t enabled);
static HAL_StatusTypeDef APP_WiFi_SendSetSupWpa2EapverIovar(int32_t eapVersion);
static HAL_StatusTypeDef APP_WiFi_SendSetSupWpaTimeoutIovar(int32_t timeoutMs);
static HAL_StatusTypeDef APP_WiFi_SendSetPassphrasePmk(const uint8_t *passphrase, uint16_t passphraseLength);
static HAL_StatusTypeDef APP_WiFi_SendJoinSsidIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendClearScanSuppressIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendSetPmOffIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendEscanIovar(void);
static void APP_WiFi_HandleAsyncEvent(const uint8_t *frame, uint16_t captured);
static uint8_t APP_WiFi_TryProbeSdpcmRxInternal(uint8_t requireInterruptHint);
static uint32_t APP_WiFi_DrainSdpcmRxQueue(uint8_t maxFrames);
static void APP_WiFi_PollPendingIoctlResponse(void);
static void APP_WiFi_PollActiveScanResults(void);
static void APP_WiFi_ProcessScanRequest(void);
static void APP_WiFi_StartScanRequest(void);
static void APP_WiFi_FailScan(const char *reason);
static void APP_WiFi_ResetNoCreditStall(void);
static void APP_WiFi_ResetControlContextForRecovery(void);
static void APP_WiFi_RecordNoCreditStall(const char *scope, const char *name);
static uint8_t APP_WiFi_ReserveControlCredit(const char *scope, const char *name, uint8_t *availableCredits);
static void APP_WiFi_ProcessJoinRequest(void);
static void APP_WiFi_ResetJoinProgress(void);
static void APP_WiFi_StartJoinAttempt(uint32_t now);
static void APP_WiFi_ProcessJoinRetry(void);
static void APP_WiFi_EvaluateJoinCompletion(void);
static void APP_WiFi_FailJoin(const char *reason, uint32_t status, uint32_t detail);
static void APP_WiFi_MarkConnectedLinkIssue(const char *source, uint32_t status, uint32_t reason);
static void APP_WiFi_ClearConnectedLinkIssue(const char *source);
static void APP_WiFi_ProcessConnectedLinkRecovery(void);

static void APP_WiFi_SetState(APP_WiFiState_t nextState)
{
  const APP_WiFiState_t previousState = g_wifiState;

  g_wifiState = nextState;

  if (previousState != nextState)
  {
    APP_WiFi_Logf("[wifi] state: %s -> %s\n",
                  APP_WiFi_StateToString(previousState),
                  APP_WiFi_StateToString(nextState));
    APP_WiFi_LogStateDetails(nextState);
  }
}

void APP_WiFi_Init(void)
{
  g_wifiOobInterruptCount = 0U;
  g_wifiLastHeartbeatTick = HAL_GetTick();
  g_wifiSdpcmFrameCount = 0U;
  g_wifiLastSdpcmFrameInterrupt = 0U;
  g_wifiLastSdpcmFrameLength = 0U;
  g_wifiLastSdpcmSequence = 0U;
  g_wifiLastSdpcmChannel = 0U;
  g_wifiLastSdpcmHeaderLength = 0U;
  g_wifiLastSdpcmNextLength = 0U;
  g_wifiLastSdpcmBusCredit = 0U;
  g_wifiLastSdpcmFlowControl = 0U;
  g_wifiBusCredit = 1U;
  g_wifiBusCreditDiff = 0U;
  g_wifiTxSequence = 0U;
  g_wifiIoctlRequestId = 0U;
  g_wifiIoctlProbeSent = 0U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiVersionProbeCompleted = 0U;
  g_wifiUpProbeSent = 0U;
  g_wifiUpProbeCompleted = 0U;
  g_wifiTxGlomSent = 0U;
  g_wifiTxGlomCompleted = 0U;
  g_wifiApstaSent = 0U;
  g_wifiApstaCompleted = 0U;
  g_wifiCountrySent = 0U;
  g_wifiCountryCompleted = 0U;
  g_wifiGmodeSent = 0U;
  g_wifiGmodeCompleted = 0U;
  g_wifiMacProbeSent = 0U;
  g_wifiMacProbeCompleted = 0U;
  g_wifiEventMaskSent = 0U;
  g_wifiEventMaskCompleted = 0U;
  g_wifiScanSuppressSent = 0U;
  g_wifiScanSuppressCompleted = 0U;
  g_wifiPmProbeSent = 0U;
  g_wifiPmProbeCompleted = 0U;
  g_wifiScanSent = 0U;
  g_wifiScanIoctlCompleted = 0U;
  g_wifiScanCompleted = 0U;
  g_wifiScanAborted = 0U;
  g_wifiScanRequested = 0U;
  g_wifiScanStep = APP_WIFI_SCAN_STEP_IDLE;
  g_wifiScanStepStartTick = 0U;
  g_wifiScanStartTick = 0U;
  memset(g_wifiLastMacAddress, 0, sizeof(g_wifiLastMacAddress));
  memset(g_wifiLastScanSsid, 0, sizeof(g_wifiLastScanSsid));
  APP_WiFi_ClearCachedScanResults();
  g_wifiScanPartialCount = 0U;
  g_wifiScanResultCount = 0U;
  g_wifiLastScanSyncId = 0U;
  g_wifiLastScanBssCount = 0U;
  g_wifiLastScanRssi = 0;
  g_wifiLastScanChannel = 0U;
  memset(g_wifiLastScanSsid, 0, sizeof(g_wifiLastScanSsid));
  APP_WiFi_ClearCachedScanResults();
  g_wifiLastScanPollTick = 0U;
  g_wifiLastIoctlCommand = 0U;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlFlags = 0U;
  g_wifiLastIoctlValue = 0U;
  g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_NONE;
  APP_WiFi_ResetNoCreditStall();
  g_wifiLinkState = APP_WIFI_LINK_STATE_IDLE;
  g_wifiJoinSecurity = APP_WIFI_JOIN_SECURITY_OPEN;
  memset(g_wifiJoinRequestedSsid, 0, sizeof(g_wifiJoinRequestedSsid));
  g_wifiJoinRequestedSsidLength = 0U;
  memset(g_wifiJoinRequestedPassphrase, 0, sizeof(g_wifiJoinRequestedPassphrase));
  g_wifiJoinRequestedPassphraseLength = 0U;
  APP_WiFi_ResetJoinProgress();
  APP_WiFi_Resources_Init();
  APP_WiFi_Platform_Init();
  APP_WiFi_SetState(APP_WIFI_STATE_IDLE);
  APP_WiFi_Logf("[wifi] init complete, debug UART ready on USART1\n");
}

APP_WiFiState_t APP_WiFi_GetState(void)
{
  return g_wifiState;
}

uint32_t APP_WiFi_GetOobInterruptCount(void)
{
  return g_wifiOobInterruptCount;
}

uint8_t APP_WiFi_IsScanComplete(void)
{
  return g_wifiScanCompleted;
}

uint8_t APP_WiFi_IsScanAborted(void)
{
  return g_wifiScanAborted;
}

uint32_t APP_WiFi_GetCachedScanResultCount(void)
{
  return g_wifiCachedScanResultCount;
}

APP_WiFiLinkState_t APP_WiFi_GetLinkState(void)
{
  return g_wifiLinkState;
}

uint32_t APP_WiFi_CopyCachedScanResults(APP_WiFiScanResult_t *results, uint32_t maxResults)
{
  uint32_t resultCount = g_wifiCachedScanResultCount;

  if ((results == NULL) || (maxResults == 0U))
  {
    return 0U;
  }

  if (resultCount > maxResults)
  {
    resultCount = maxResults;
  }

  memcpy(results, g_wifiCachedScanResults, resultCount * sizeof(APP_WiFiScanResult_t));
  return resultCount;
}

uint8_t APP_WiFi_RequestScan(void)
{
  const uint8_t scanActive = ((g_wifiScanStep != APP_WIFI_SCAN_STEP_IDLE) ||
                              ((g_wifiScanIoctlCompleted != 0U) &&
                               (g_wifiScanCompleted == 0U) &&
                               (g_wifiScanAborted == 0U))) ? 1U : 0U;

  if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING)
  {
    APP_WiFi_Logf("[wifi] scan: request ignored while join is in progress\n");
    return 0U;
  }

  if (scanActive != 0U)
  {
    return 1U;
  }

  g_wifiScanRequested = 1U;
  g_wifiScanCompleted = 0U;
  g_wifiScanAborted = 0U;
  APP_WiFi_Logf("[wifi] scan: requested\n");
  return 1U;
}

uint8_t APP_WiFi_RequestJoin(const char *ssid, const char *password)
{
  size_t ssidLength = 0U;
  size_t passwordLength = 0U;

  if (ssid == NULL)
  {
    return 0U;
  }

  ssidLength = strnlen(ssid, APP_WIFI_JOIN_MAX_SSID_LEN + 1U);
  if ((ssidLength == 0U) || (ssidLength > APP_WIFI_JOIN_MAX_SSID_LEN))
  {
    APP_WiFi_Logf("[wifi] join: invalid ssid length=%lu\n", (unsigned long)ssidLength);
    return 0U;
  }

  passwordLength = (password != NULL) ? strnlen(password, APP_WIFI_JOIN_MAX_PASSWORD_LEN + 1U) : 0U;
  if (passwordLength > APP_WIFI_JOIN_MAX_PASSWORD_LEN)
  {
    APP_WiFi_Logf("[wifi] join: invalid password length=%lu\n", (unsigned long)passwordLength);
    return 0U;
  }

  if ((passwordLength != 0U) && (passwordLength < APP_WIFI_WSEC_MIN_PSK_LEN))
  {
    APP_WiFi_Logf("[wifi] join: password too short len=%lu (min=%u)\n",
                  (unsigned long)passwordLength,
                  (unsigned int)APP_WIFI_WSEC_MIN_PSK_LEN);
    return 0U;
  }

  if ((g_wifiState != APP_WIFI_STATE_MAILBOX_READY) ||
      (g_wifiEventMaskCompleted == 0U) ||
      (g_wifiPmProbeCompleted == 0U))
  {
    APP_WiFi_Logf("[wifi] join: wifi stack not ready state=%s evt=%u pm=%u\n",
                  APP_WiFi_StateToString(g_wifiState),
                  (unsigned int)g_wifiEventMaskCompleted,
                  (unsigned int)g_wifiPmProbeCompleted);
    return 0U;
  }

  if ((g_wifiScanIoctlCompleted != 0U) &&
      (g_wifiScanCompleted == 0U) &&
      (g_wifiScanAborted == 0U))
  {
    APP_WiFi_Logf("[wifi] join: active scan still running, please retry after completion\n");
    return 0U;
  }

  if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING)
  {
    APP_WiFi_Logf("[wifi] join: request ignored, another join is already in progress\n");
    return 0U;
  }

  memset(g_wifiJoinRequestedSsid, 0, sizeof(g_wifiJoinRequestedSsid));
  memcpy(g_wifiJoinRequestedSsid, ssid, ssidLength);
  g_wifiJoinRequestedSsidLength = (uint8_t)ssidLength;

  memset(g_wifiJoinRequestedPassphrase, 0, sizeof(g_wifiJoinRequestedPassphrase));
  if (passwordLength != 0U)
  {
    memcpy(g_wifiJoinRequestedPassphrase, password, passwordLength);
  }
  g_wifiJoinRequestedPassphraseLength = (uint8_t)passwordLength;

  g_wifiJoinSecurity = (passwordLength == 0U) ? APP_WIFI_JOIN_SECURITY_OPEN : APP_WIFI_JOIN_SECURITY_WPA2_PSK;
  g_wifiJoinRetryCount = 0U;
  g_wifiJoinRetryAtTick = 0U;
  APP_WiFi_StartJoinAttempt(HAL_GetTick());

  APP_WiFi_Logf("[wifi] join: requested ssid=\"%s\" security=%s passLen=%u\n",
                g_wifiJoinRequestedSsid,
                (g_wifiJoinSecurity == APP_WIFI_JOIN_SECURITY_OPEN) ? "open" : "wpa2-psk",
                (unsigned int)g_wifiJoinRequestedPassphraseLength);
  return 1U;
}

void APP_WiFi_HandleOobInterrupt(uint16_t gpioPin)
{
  if (gpioPin == WIFI_OOB_IRQ_Pin)
  {
    g_wifiOobInterruptCount++;
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  APP_WiFi_HandleOobInterrupt(GPIO_Pin);
}

static const char *APP_WiFi_StateToString(APP_WiFiState_t state)
{
  switch (state)
  {
    case APP_WIFI_STATE_IDLE:
      return "IDLE";
    case APP_WIFI_STATE_WAIT_STACK:
      return "WAIT_STACK";
    case APP_WIFI_STATE_RESET_ASSERT:
      return "RESET_ASSERT";
    case APP_WIFI_STATE_RESET_RELEASE:
      return "RESET_RELEASE";
    case APP_WIFI_STATE_MODULE_SETTLE:
      return "MODULE_SETTLE";
    case APP_WIFI_STATE_BRINGUP_PENDING:
      return "BRINGUP_PENDING";
    case APP_WIFI_STATE_SDIO_HOST_READY:
      return "SDIO_HOST_READY";
    case APP_WIFI_STATE_SDIO_ENUMERATED:
      return "SDIO_ENUMERATED";
    case APP_WIFI_STATE_CCCR_READY:
      return "CCCR_READY";
    case APP_WIFI_STATE_FUNCTION1_READY:
      return "FUNCTION1_READY";
    case APP_WIFI_STATE_BUS_READY:
      return "BUS_READY";
    case APP_WIFI_STATE_CMD53_READY:
      return "CMD53_READY";
    case APP_WIFI_STATE_CLOCK_READY:
      return "CLOCK_READY";
    case APP_WIFI_STATE_BACKPLANE_READY:
      return "BACKPLANE_READY";
    case APP_WIFI_STATE_HT_CLOCK_READY:
      return "HT_CLOCK_READY";
    case APP_WIFI_STATE_FUNCTION2_READY:
      return "FUNCTION2_READY";
    case APP_WIFI_STATE_INTERRUPTS_READY:
      return "INTERRUPTS_READY";
    case APP_WIFI_STATE_RESOURCES_READY:
      return "RESOURCES_READY";
    case APP_WIFI_STATE_FIRMWARE_STAGED:
      return "FIRMWARE_STAGED";
    case APP_WIFI_STATE_NVRAM_STAGED:
      return "NVRAM_STAGED";
    case APP_WIFI_STATE_ARM_RELEASED:
      return "ARM_RELEASED";
    case APP_WIFI_STATE_FIRMWARE_BOOTED:
      return "FIRMWARE_BOOTED";
    case APP_WIFI_STATE_SHARED_READY:
      return "SHARED_READY";
    case APP_WIFI_STATE_CONSOLE_READY:
      return "CONSOLE_READY";
    case APP_WIFI_STATE_MAILBOX_READY:
      return "MAILBOX_READY";
    case APP_WIFI_STATE_READY:
      return "READY";
    case APP_WIFI_STATE_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

static const char *APP_WiFi_SdpcmChannelToString(uint8_t channel)
{
  switch (channel)
  {
    case APP_WIFI_SDPCM_CHANNEL_CONTROL:
      return "control";
    case APP_WIFI_SDPCM_CHANNEL_EVENT:
      return "event";
    case APP_WIFI_SDPCM_CHANNEL_DATA:
      return "data";
    default:
      return "other";
  }
}

static void APP_WiFi_Logf(const char *format, ...)
{
  char logBuffer[APP_WIFI_LOG_BUFFER_SIZE];
  int written = 0;
  va_list args;

  if (format == NULL)
  {
    return;
  }

  va_start(args, format);
  written = vsnprintf(logBuffer, sizeof(logBuffer), format, args);
  va_end(args);

  if (written <= 0)
  {
    return;
  }

  if ((size_t)written >= sizeof(logBuffer))
  {
    logBuffer[sizeof(logBuffer) - 2U] = '\n';
    logBuffer[sizeof(logBuffer) - 1U] = '\0';
  }

  (void)APP_DebugUart_WriteString(logBuffer);
}

static uint16_t APP_WiFi_ReadLe16(const uint8_t *bytes)
{
  if (bytes == NULL)
  {
    return 0U;
  }

  return (uint16_t)(((uint16_t)bytes[0]) | ((uint16_t)bytes[1] << 8U));
}

static uint32_t APP_WiFi_ReadLe32(const uint8_t *bytes)
{
  if (bytes == NULL)
  {
    return 0U;
  }

  return ((uint32_t)bytes[0]) |
         ((uint32_t)bytes[1] << 8U) |
         ((uint32_t)bytes[2] << 16U) |
         ((uint32_t)bytes[3] << 24U);
}

static uint16_t APP_WiFi_ReadBe16(const uint8_t *bytes)
{
  if (bytes == NULL)
  {
    return 0U;
  }

  return (uint16_t)(((uint16_t)bytes[0] << 8U) | (uint16_t)bytes[1]);
}

static uint32_t APP_WiFi_ReadBe32(const uint8_t *bytes)
{
  if (bytes == NULL)
  {
    return 0U;
  }

  return ((uint32_t)bytes[0] << 24U) |
         ((uint32_t)bytes[1] << 16U) |
         ((uint32_t)bytes[2] << 8U) |
         (uint32_t)bytes[3];
}

static void APP_WiFi_WriteLe16(uint8_t *bytes, uint16_t value)
{
  if (bytes == NULL)
  {
    return;
  }

  bytes[0] = (uint8_t)(value & 0xFFU);
  bytes[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void APP_WiFi_WriteLe32(uint8_t *bytes, uint32_t value)
{
  if (bytes == NULL)
  {
    return;
  }

  bytes[0] = (uint8_t)(value & 0xFFU);
  bytes[1] = (uint8_t)((value >> 8U) & 0xFFU);
  bytes[2] = (uint8_t)((value >> 16U) & 0xFFU);
  bytes[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static uint16_t APP_WiFi_GetSdpcmChunkSize(uint16_t remaining)
{
  return (remaining > 4U) ? 4U : remaining;
}

static void APP_WiFi_RecordSdpcmHeader(const uint8_t *header, uint16_t frameLength, uint32_t frameInterrupt)
{
  if (header == NULL)
  {
    return;
  }

  g_wifiSdpcmFrameCount++;
  g_wifiLastSdpcmFrameInterrupt = frameInterrupt;
  g_wifiLastSdpcmFrameLength = frameLength;
  g_wifiLastSdpcmSequence = header[4];
  g_wifiLastSdpcmChannel = (uint8_t)(header[5] & APP_WIFI_SDPCM_CHANNEL_MASK);
  g_wifiLastSdpcmNextLength = header[6];
  g_wifiLastSdpcmHeaderLength = header[7];
  g_wifiLastSdpcmFlowControl = header[8];
  g_wifiLastSdpcmBusCredit = header[9];

  if (APP_WIFI_SDPCM_TRACE_ENABLE != 0U)
  {
    APP_WiFi_Logf("[wifi] sdpcm: #%lu irq=0x%08lX len=%u seq=%u ch=%s(%u) hdr=%u next=%u credit=%u flow=%u\n",
                  (unsigned long)g_wifiSdpcmFrameCount,
                  (unsigned long)frameInterrupt,
                  (unsigned int)frameLength,
                  (unsigned int)g_wifiLastSdpcmSequence,
                  APP_WiFi_SdpcmChannelToString(g_wifiLastSdpcmChannel),
                  (unsigned int)g_wifiLastSdpcmChannel,
                  (unsigned int)g_wifiLastSdpcmHeaderLength,
                  (unsigned int)g_wifiLastSdpcmNextLength,
                  (unsigned int)g_wifiLastSdpcmBusCredit,
                  (unsigned int)g_wifiLastSdpcmFlowControl);
  }
}

static void APP_WiFi_UpdateSdpcmCredit(const uint8_t *header)
{
  uint8_t creditDiff = 0U;

  if (header == NULL)
  {
    return;
  }

  if ((header[5] & APP_WIFI_SDPCM_CHANNEL_MASK) >= 3U)
  {
    return;
  }

  creditDiff = (uint8_t)(header[9] - g_wifiBusCredit);
  if (creditDiff <= 15U)
  {
    g_wifiBusCredit = header[9];
    g_wifiBusCreditDiff = creditDiff;

    if ((uint8_t)(g_wifiBusCredit - g_wifiTxSequence) != 0U)
    {
      APP_WiFi_ResetNoCreditStall();
    }
  }
}

static void APP_WiFi_ResetNoCreditStall(void)
{
  g_wifiNoCreditHitCount = 0U;
  g_wifiNoCreditTxSequenceSnapshot = 0xFFU;
  g_wifiNoCreditBusCreditSnapshot = 0xFFU;
  g_wifiNoCreditFirstTick = 0U;
  g_wifiNoCreditLastTick = 0U;
}

static void APP_WiFi_ResetControlContextForRecovery(void)
{
  const uint8_t hasJoinProfile = (g_wifiJoinRequestedSsidLength != 0U) ? 1U : 0U;

  g_wifiBusCredit = 1U;
  g_wifiBusCreditDiff = 0U;
  g_wifiTxSequence = 0U;
  g_wifiIoctlRequestId = 0U;
  g_wifiIoctlProbeSent = 0U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiVersionProbeCompleted = 0U;
  g_wifiUpProbeSent = 0U;
  g_wifiUpProbeCompleted = 0U;
  g_wifiTxGlomSent = 0U;
  g_wifiTxGlomCompleted = 0U;
  g_wifiApstaSent = 0U;
  g_wifiApstaCompleted = 0U;
  g_wifiCountrySent = 0U;
  g_wifiCountryCompleted = 0U;
  g_wifiGmodeSent = 0U;
  g_wifiGmodeCompleted = 0U;
  g_wifiMacProbeSent = 0U;
  g_wifiMacProbeCompleted = 0U;
  g_wifiEventMaskSent = 0U;
  g_wifiEventMaskCompleted = 0U;
  g_wifiScanSuppressSent = 0U;
  g_wifiScanSuppressCompleted = 0U;
  g_wifiPmProbeSent = 0U;
  g_wifiPmProbeCompleted = 0U;
  g_wifiScanSent = 0U;
  g_wifiScanIoctlCompleted = 0U;
  g_wifiScanCompleted = 0U;
  g_wifiScanAborted = 0U;
  g_wifiScanRequested = 0U;
  g_wifiScanStep = APP_WIFI_SCAN_STEP_IDLE;
  g_wifiScanStepStartTick = 0U;
  g_wifiScanStartTick = 0U;
  g_wifiLastScanPollTick = 0U;
  g_wifiLastIoctlCommand = 0U;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlFlags = 0U;
  g_wifiLastIoctlValue = 0U;
  g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_NONE;
  APP_WiFi_ResetJoinProgress();
  APP_WiFi_ResetNoCreditStall();

  if (hasJoinProfile != 0U)
  {
    g_wifiLinkState = APP_WIFI_LINK_STATE_FAILED;
    g_wifiJoinRetryCount = 0U;
    g_wifiJoinRetryAtTick = HAL_GetTick() + APP_WIFI_RECOVERY_JOIN_DELAY_MS;
    APP_WiFi_Logf("[wifi] recovery: join retry scheduled in %lu ms ssid=\"%s\"\n",
                  (unsigned long)APP_WIFI_RECOVERY_JOIN_DELAY_MS,
                  g_wifiJoinRequestedSsid);
  }
  else
  {
    g_wifiLinkState = APP_WIFI_LINK_STATE_IDLE;
    g_wifiJoinRetryCount = 0U;
    g_wifiJoinRetryAtTick = 0U;
  }
}

static void APP_WiFi_RecordNoCreditStall(const char *scope, const char *name)
{
  const uint32_t now = HAL_GetTick();
  const uint8_t txSnapshot = g_wifiTxSequence;
  const uint8_t creditSnapshot = g_wifiBusCredit;

  if ((g_wifiNoCreditHitCount == 0U) ||
      (g_wifiNoCreditTxSequenceSnapshot != txSnapshot) ||
      (g_wifiNoCreditBusCreditSnapshot != creditSnapshot) ||
      ((now - g_wifiNoCreditLastTick) > APP_WIFI_CREDIT_STALL_GAP_MS))
  {
    g_wifiNoCreditHitCount = 1U;
    g_wifiNoCreditFirstTick = now;
    g_wifiNoCreditTxSequenceSnapshot = txSnapshot;
    g_wifiNoCreditBusCreditSnapshot = creditSnapshot;
  }
  else if (g_wifiNoCreditHitCount < 0xFFU)
  {
    g_wifiNoCreditHitCount++;
  }

  g_wifiNoCreditLastTick = now;

  if ((g_wifiState == APP_WIFI_STATE_MAILBOX_READY) &&
      (g_wifiNoCreditHitCount >= APP_WIFI_CREDIT_STALL_MIN_HITS) &&
      ((now - g_wifiNoCreditFirstTick) >= APP_WIFI_CREDIT_STALL_WINDOW_MS))
  {
    APP_WiFi_Logf("[wifi] credit: stall scope=%s name=%s tx=%u credit=%u hits=%u -> reset stack\n",
                  (scope != NULL) ? scope : "unknown",
                  (name != NULL) ? name : "unknown",
                  (unsigned int)txSnapshot,
                  (unsigned int)creditSnapshot,
                  (unsigned int)g_wifiNoCreditHitCount);

    (void)APP_WiFi_Platform_AbortFunction2Read();
    APP_WiFi_ResetControlContextForRecovery();
    APP_WiFi_SetState(APP_WIFI_STATE_RESET_ASSERT);
  }
}

static uint8_t APP_WiFi_ReserveControlCredit(const char *scope, const char *name, uint8_t *availableCredits)
{
  uint8_t available = (uint8_t)(g_wifiBusCredit - g_wifiTxSequence);

  if (available == 0U)
  {
    if (g_wifiState == APP_WIFI_STATE_MAILBOX_READY)
    {
      (void)APP_WiFi_DrainSdpcmRxQueue(1U);
      available = (uint8_t)(g_wifiBusCredit - g_wifiTxSequence);
    }

    if (available == 0U)
    {
      APP_WiFi_RecordNoCreditStall(scope, name);
      return 0U;
    }
  }

  APP_WiFi_ResetNoCreditStall();
  if (availableCredits != NULL)
  {
    *availableCredits = available;
  }
  return 1U;
}

static void APP_WiFi_LogSdpcmBytes(const char *prefix, const uint8_t *data, uint16_t length)
{
  char logBuffer[APP_WIFI_LOG_BUFFER_SIZE];
  int written = 0;
  uint16_t index = 0U;

  if ((prefix == NULL) || (data == NULL) || (length == 0U))
  {
    return;
  }

  written = snprintf(logBuffer, sizeof(logBuffer), "%s", prefix);
  if ((written <= 0) || ((size_t)written >= sizeof(logBuffer)))
  {
    return;
  }

  for (index = 0U; index < length; ++index)
  {
    const int chunkWritten = snprintf(&logBuffer[written],
                                      sizeof(logBuffer) - (size_t)written,
                                      "%s%02X",
                                      (index == 0U) ? "" : " ",
                                      (unsigned int)data[index]);
    if ((chunkWritten <= 0) || ((size_t)chunkWritten >= (sizeof(logBuffer) - (size_t)written)))
    {
      break;
    }

    written += chunkWritten;
  }

  if ((size_t)written < (sizeof(logBuffer) - 2U))
  {
    logBuffer[written++] = '\n';
    logBuffer[written] = '\0';
  }
  else
  {
    logBuffer[sizeof(logBuffer) - 2U] = '\n';
    logBuffer[sizeof(logBuffer) - 1U] = '\0';
  }

  (void)APP_DebugUart_WriteString(logBuffer);
}

static void APP_WiFi_LogMacAddress(const char *prefix, const uint8_t *macAddress)
{
  if ((prefix == NULL) || (macAddress == NULL))
  {
    return;
  }

  APP_WiFi_Logf("%s%02X:%02X:%02X:%02X:%02X:%02X\n",
                prefix,
                (unsigned int)macAddress[0],
                (unsigned int)macAddress[1],
                (unsigned int)macAddress[2],
                (unsigned int)macAddress[3],
                (unsigned int)macAddress[4],
                (unsigned int)macAddress[5]);
}

static int APP_WiFi_ParseHexNibble(char character)
{
  if ((character >= '0') && (character <= '9'))
  {
    return (int)(character - '0');
  }

  if ((character >= 'a') && (character <= 'f'))
  {
    return 10 + (int)(character - 'a');
  }

  if ((character >= 'A') && (character <= 'F'))
  {
    return 10 + (int)(character - 'A');
  }

  return -1;
}

static uint8_t APP_WiFi_ParseNvramMacAddress(const char *text, uint8_t *macAddress)
{
  const char *cursor = NULL;

  if ((text == NULL) || (macAddress == NULL))
  {
    return 0U;
  }

  cursor = strstr(text, "macaddr=");
  if (cursor == NULL)
  {
    return 0U;
  }

  cursor += strlen("macaddr=");

  for (uint8_t index = 0U; index < APP_WIFI_MAC_ADDRESS_SIZE; ++index)
  {
    const int high = APP_WiFi_ParseHexNibble(cursor[0]);
    const int low = APP_WiFi_ParseHexNibble(cursor[1]);

    if ((high < 0) || (low < 0))
    {
      return 0U;
    }

    macAddress[index] = (uint8_t)(((uint8_t)high << 4U) | (uint8_t)low);
    cursor += 2;

    if (index < (APP_WIFI_MAC_ADDRESS_SIZE - 1U))
    {
      if ((*cursor != ':') && (*cursor != '-'))
      {
        return 0U;
      }

      ++cursor;
    }
  }

  return 1U;
}

static uint8_t APP_WiFi_IsMacAddressUsable(const uint8_t *macAddress)
{
  uint8_t allZero = 1U;
  uint8_t allFF = 1U;

  if (macAddress == NULL)
  {
    return 0U;
  }

  for (uint8_t index = 0U; index < APP_WIFI_MAC_ADDRESS_SIZE; ++index)
  {
    if (macAddress[index] != 0x00U)
    {
      allZero = 0U;
    }

    if (macAddress[index] != 0xFFU)
    {
      allFF = 0U;
    }
  }

  return (uint8_t)((allZero == 0U) && (allFF == 0U));
}

static void APP_WiFi_GenerateFallbackMacAddress(uint8_t *macAddress)
{
  const uint32_t uid0 = HAL_GetUIDw0();
  const uint32_t uid1 = HAL_GetUIDw1();
  const uint32_t uid2 = HAL_GetUIDw2();

  if (macAddress == NULL)
  {
    return;
  }

  macAddress[0] = 0x02U;
  macAddress[1] = (uint8_t)(uid0 & 0xFFU);
  macAddress[2] = (uint8_t)((uid0 >> 8U) & 0xFFU);
  macAddress[3] = (uint8_t)(uid1 & 0xFFU);
  macAddress[4] = (uint8_t)((uid1 >> 8U) & 0xFFU);
  macAddress[5] = (uint8_t)(uid2 & 0xFFU);
}

uint8_t APP_WiFi_GetMacAddress(uint8_t *macAddress)
{
  const uint8_t *nvramData = NULL;
  uint32_t nvramSize = 0U;

  if (macAddress == NULL)
  {
    return 0U;
  }

  if (APP_WiFi_IsMacAddressUsable(g_wifiLastMacAddress) != 0U)
  {
    memcpy(macAddress, g_wifiLastMacAddress, APP_WIFI_MAC_ADDRESS_SIZE);
    return 1U;
  }

  if ((APP_WiFi_Resources_GetNvram(&nvramData, &nvramSize) == HAL_OK) &&
      (nvramData != NULL) &&
      (nvramSize != 0U) &&
      (APP_WiFi_ParseNvramMacAddress((const char *)nvramData, macAddress) != 0U))
  {
    memcpy(g_wifiLastMacAddress, macAddress, APP_WIFI_MAC_ADDRESS_SIZE);
    return 1U;
  }

  APP_WiFi_GenerateFallbackMacAddress(macAddress);
  memcpy(g_wifiLastMacAddress, macAddress, APP_WIFI_MAC_ADDRESS_SIZE);
  return 1U;
}

static void APP_WiFi_SetEventMaskBit(uint8_t *eventMask, uint16_t eventNumber)
{
  if ((eventMask == NULL) || (eventNumber >= (APP_WIFI_WL_EVENTING_MASK_LEN * 8U)))
  {
    return;
  }

  eventMask[eventNumber / 8U] |= (uint8_t)(1U << (eventNumber % 8U));
}

static void APP_WiFi_CopyScanSsid(char *destination, const uint8_t *ssid, uint8_t ssidLength)
{
  uint8_t index = 0U;
  uint8_t length = ssidLength;

  if ((destination == NULL) || (ssid == NULL))
  {
    return;
  }

  if (length > 32U)
  {
    length = 32U;
  }

  for (index = 0U; index < length; ++index)
  {
    const uint8_t character = ssid[index];
    destination[index] = ((character >= 32U) && (character <= 126U)) ? (char)character : '.';
  }

  destination[length] = '\0';
}

static void APP_WiFi_ClearCachedScanResults(void)
{
  memset(g_wifiCachedScanResults, 0, sizeof(g_wifiCachedScanResults));
  g_wifiCachedScanResultCount = 0U;
}

static int32_t APP_WiFi_FindCachedScanResult(const uint8_t *bssid)
{
  uint32_t index = 0U;

  if (bssid == NULL)
  {
    return -1;
  }

  for (index = 0U; index < g_wifiCachedScanResultCount; ++index)
  {
    if (memcmp(g_wifiCachedScanResults[index].bssid, bssid, APP_WIFI_MAC_ADDRESS_SIZE) == 0)
    {
      return (int32_t)index;
    }
  }

  return -1;
}

static void APP_WiFi_UpdateCachedScanResult(const APP_WiFi_BssInfo_t *bssInfo)
{
  APP_WiFiScanResult_t *cachedResult = NULL;
  int32_t resultIndex = -1;
  const uint8_t channel = (bssInfo->ctl_ch != 0U) ? bssInfo->ctl_ch : (uint8_t)(bssInfo->chanspec & 0xFFU);

  if ((bssInfo == NULL) || (bssInfo->version != APP_WIFI_WL_BSS_INFO_VERSION))
  {
    return;
  }

  resultIndex = APP_WiFi_FindCachedScanResult(bssInfo->bssid.octet);
  if (resultIndex >= 0)
  {
    cachedResult = &g_wifiCachedScanResults[resultIndex];
  }
  else if (g_wifiCachedScanResultCount < APP_WIFI_SCAN_RESULT_CACHE_SIZE)
  {
    cachedResult = &g_wifiCachedScanResults[g_wifiCachedScanResultCount++];
    memset(cachedResult, 0, sizeof(*cachedResult));
    memcpy(cachedResult->bssid, bssInfo->bssid.octet, APP_WIFI_MAC_ADDRESS_SIZE);
  }
  else
  {
    return;
  }

  if ((cachedResult->ssid[0] == '\0') || (bssInfo->rssi >= cachedResult->rssi))
  {
    APP_WiFi_CopyScanSsid(cachedResult->ssid, bssInfo->ssid, bssInfo->ssid_length);
    cachedResult->rssi = bssInfo->rssi;
    cachedResult->channel = channel;
  }
  else if ((cachedResult->channel == 0U) && (channel != 0U))
  {
    cachedResult->channel = channel;
  }
}

static HAL_StatusTypeDef APP_WiFi_SendBufferedControlIoctl(uint8_t ioctlType,
                                                           uint32_t command,
                                                           const void *payload,
                                                           uint16_t payloadLength,
                                                           const char *name)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + 128U] = {0};
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);

  if (payloadLength > 128U)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_ReserveControlCredit("ioctl", name, &availableCredits) == 0U)
  {
    APP_WiFi_Logf("[wifi] ioctl: no credits available for %s tx=%u credit=%u\n",
                  name,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], command);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           ioctlType);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  if ((payload != NULL) && (payloadLength != 0U))
  {
    memcpy(&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE], payload, payloadLength);
  }

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] ioctl: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  name,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_NONE;
  g_wifiLastIoctlCommand = command;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = 0U;
  APP_WiFi_Logf("[wifi] ioctl: sent %s id=%u txseq=%u credits=%u len=%u type=%u\n",
                name,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                (unsigned int)payloadLength,
                (unsigned int)ioctlType);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendControlIoctl(uint8_t ioctlType,
                                                   uint32_t command,
                                                   const void *payload,
                                                   uint16_t payloadLength,
                                                   const char *name)
{
  if (payloadLength > APP_WIFI_IOCTL_U32_SIZE)
  {
    return HAL_ERROR;
  }

  return APP_WiFi_SendBufferedControlIoctl(ioctlType, command, payload, payloadLength, name);
}

static HAL_StatusTypeDef APP_WiFi_SendGetVersionIoctl(void)
{
  const uint32_t versionPlaceholder = 0U;

  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_GET,
                                   APP_WIFI_WLC_GET_VERSION,
                                   &versionPlaceholder,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_GET_VERSION");
}

static HAL_StatusTypeDef APP_WiFi_SendUpIoctl(void)
{
  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_UP,
                                   NULL,
                                   0U,
                                   "WLC_UP");
}

static HAL_StatusTypeDef APP_WiFi_SendSetU32Iovar(const char *iovarName,
                                                  uint32_t value,
                                                  const char *logName,
                                                  APP_WiFi_IovarRequest_t requestType)
{
  uint8_t payload[64U] = {0};
  const uint16_t nameLength = (uint16_t)(strlen(iovarName) + 1U);
  const uint16_t payloadLength = (uint16_t)(nameLength + sizeof(uint32_t));
  HAL_StatusTypeDef status = HAL_ERROR;

  if ((iovarName == NULL) || (logName == NULL))
  {
    return HAL_ERROR;
  }

  if (payloadLength > sizeof(payload))
  {
    return HAL_ERROR;
  }

  memcpy(payload, iovarName, nameLength);
  APP_WiFi_WriteLe32(&payload[nameLength], value);

  status = APP_WiFi_SendBufferedControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                             APP_WIFI_WLC_SET_VAR,
                                             payload,
                                             payloadLength,
                                             logName);
  if (status != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] iovar: %s payload=%u value=%lu send path failed\n",
                  logName,
                  (unsigned int)payloadLength,
                  (unsigned long)value);
    return status;
  }

  g_wifiPendingIovarRequest = requestType;
  g_wifiLastIoctlValue = value;
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendDisableTxGlomIovar(void)
{
  return APP_WiFi_SendSetU32Iovar(APP_WIFI_IOVAR_TX_GLOM,
                                  0U,
                                  APP_WIFI_IOVAR_TX_GLOM,
                                  APP_WIFI_IOVAR_REQUEST_TX_GLOM_SET);
}

static HAL_StatusTypeDef APP_WiFi_SendEnableApstaIovar(void)
{
  return APP_WiFi_SendSetU32Iovar(APP_WIFI_IOVAR_APSTA,
                                  APP_WIFI_APSTA_ENABLE,
                                  APP_WIFI_IOVAR_APSTA,
                                  APP_WIFI_IOVAR_REQUEST_APSTA_SET);
}

static HAL_StatusTypeDef APP_WiFi_SendSetCountryIovar(void)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + 80U] = {0};
  const uint16_t nameLength = (uint16_t)(strlen(APP_WIFI_IOVAR_COUNTRY) + 1U);
  const uint16_t payloadLength = (uint16_t)(nameLength + sizeof(APP_WiFi_Country_t));
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);
  APP_WiFi_Country_t *country = NULL;
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;

  if (APP_WiFi_ReserveControlCredit("iovar", APP_WIFI_IOVAR_COUNTRY, &availableCredits) == 0U)
  {
    APP_WiFi_Logf("[wifi] iovar: no credits available for %s tx=%u credit=%u\n",
                  APP_WIFI_IOVAR_COUNTRY,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], APP_WIFI_WLC_SET_VAR);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           APP_WIFI_SDPCM_IOCTL_SET);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  memcpy(&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
         APP_WIFI_IOVAR_COUNTRY,
         nameLength);

  country = (APP_WiFi_Country_t *)&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + nameLength];
  memset(country, 0, sizeof(*country));
  memcpy(country->country_abbrev, APP_WIFI_COUNTRY_CODE, 2U);
  memcpy(country->ccode, APP_WIFI_COUNTRY_CODE, 2U);
  country->rev = APP_WIFI_COUNTRY_REV;

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] iovar: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  APP_WIFI_IOVAR_COUNTRY,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_COUNTRY_SET;
  g_wifiLastIoctlCommand = APP_WIFI_WLC_SET_VAR;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = 0U;
  APP_WiFi_Logf("[wifi] iovar: sent %s id=%u txseq=%u credits=%u code=%s rev=%ld\n",
                APP_WIFI_IOVAR_COUNTRY,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                APP_WIFI_COUNTRY_CODE,
                (long)APP_WIFI_COUNTRY_REV);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendSetGmodeAutoIoctl(void)
{
  const uint32_t gmode = APP_WIFI_GMODE_AUTO;

  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_SET_GMODE,
                                   &gmode,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_SET_GMODE(GMODE_AUTO)");
}

static HAL_StatusTypeDef APP_WiFi_SendGetCurEtheraddrIovar(void)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + 32U] = {0};
  const char *iovarName = APP_WIFI_IOVAR_CUR_ETHERADDR;
  const uint16_t nameLength = (uint16_t)(strlen(iovarName) + 1U);
  const uint16_t payloadLength = (uint16_t)(nameLength + APP_WIFI_MAC_ADDRESS_SIZE);
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;

  if (APP_WiFi_ReserveControlCredit("iovar", APP_WIFI_IOVAR_CUR_ETHERADDR, &availableCredits) == 0U)
  {
    APP_WiFi_Logf("[wifi] iovar: no credits available for %s tx=%u credit=%u\n",
                  APP_WIFI_IOVAR_CUR_ETHERADDR,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], APP_WIFI_WLC_GET_VAR);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           APP_WIFI_SDPCM_IOCTL_GET);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  memcpy(&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
         iovarName,
         nameLength);

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] iovar: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  APP_WIFI_IOVAR_CUR_ETHERADDR,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_CUR_ETHERADDR_GET;
  g_wifiLastIoctlCommand = APP_WIFI_WLC_GET_VAR;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = 0U;
  APP_WiFi_Logf("[wifi] iovar: sent %s id=%u txseq=%u credits=%u len=%u\n",
                APP_WIFI_IOVAR_CUR_ETHERADDR,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                (unsigned int)APP_WIFI_MAC_ADDRESS_SIZE);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendSetEventMsgsIovar(void)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + 64U] = {0};
  const char *iovarName = APP_WIFI_IOVAR_BSSCFG_EVENT_MSGS;
  const uint16_t nameLength = (uint16_t)(strlen(iovarName) + 1U);
  const uint16_t payloadLength = (uint16_t)(nameLength + sizeof(uint32_t) + APP_WIFI_WL_EVENTING_MASK_LEN);
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);
  uint8_t *bssIndex = &frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + nameLength];
  uint8_t *eventMask = &bssIndex[sizeof(uint32_t)];
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;

  if (APP_WiFi_ReserveControlCredit("iovar", APP_WIFI_IOVAR_EVENT_MSGS, &availableCredits) == 0U)
  {
    APP_WiFi_Logf("[wifi] iovar: no credits available for %s tx=%u credit=%u\n",
                  APP_WIFI_IOVAR_EVENT_MSGS,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], APP_WIFI_WLC_SET_VAR);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           APP_WIFI_SDPCM_IOCTL_SET);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  memcpy(&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
         iovarName,
         nameLength);
  APP_WiFi_WriteLe32(bssIndex, APP_WIFI_STA_BSSCFG_INDEX);

  memset(eventMask, 0, APP_WIFI_WL_EVENTING_MASK_LEN);
  APP_WiFi_SetEventMaskBit(eventMask, 0U);   /* WLC_E_SET_SSID */
  APP_WiFi_SetEventMaskBit(eventMask, 1U);   /* WLC_E_JOIN */
  APP_WiFi_SetEventMaskBit(eventMask, 3U);   /* WLC_E_AUTH */
  APP_WiFi_SetEventMaskBit(eventMask, 5U);   /* WLC_E_DEAUTH */
  APP_WiFi_SetEventMaskBit(eventMask, 6U);   /* WLC_E_DEAUTH_IND */
  APP_WiFi_SetEventMaskBit(eventMask, 7U);   /* WLC_E_ASSOC */
  APP_WiFi_SetEventMaskBit(eventMask, 11U);  /* WLC_E_DISASSOC */
  APP_WiFi_SetEventMaskBit(eventMask, 12U);  /* WLC_E_DISASSOC_IND */
  APP_WiFi_SetEventMaskBit(eventMask, 16U);  /* WLC_E_LINK */
  APP_WiFi_SetEventMaskBit(eventMask, 23U);  /* WLC_E_PRUNE */
  APP_WiFi_SetEventMaskBit(eventMask, 26U);  /* WLC_E_SCAN_COMPLETE */
  APP_WiFi_SetEventMaskBit(eventMask, 46U);  /* WLC_E_PSK_SUP */
  APP_WiFi_SetEventMaskBit(eventMask, 69U);  /* WLC_E_ESCAN_RESULT */

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] iovar: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  APP_WIFI_IOVAR_EVENT_MSGS,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_EVENT_MSGS_SET;
  g_wifiLastIoctlCommand = APP_WIFI_WLC_SET_VAR;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = 0U;
  APP_WiFi_Logf("[wifi] iovar: sent %s id=%u txseq=%u credits=%u maskLen=%u bss=%lu\n",
                APP_WIFI_IOVAR_EVENT_MSGS,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                (unsigned int)APP_WIFI_WL_EVENTING_MASK_LEN,
                (unsigned long)APP_WIFI_STA_BSSCFG_INDEX);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendSetBsscfgU32Iovar(const char *iovarName,
                                                        int32_t value,
                                                        const char *logName,
                                                        APP_WiFi_IovarRequest_t requestType)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + 64U] = {0};
  const uint16_t nameLength = (uint16_t)(strlen(iovarName) + 1U);
  const uint16_t payloadLength = (uint16_t)(nameLength + sizeof(uint32_t) + sizeof(uint32_t));
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);
  uint8_t *payload = NULL;
  uint8_t *bssIndex = NULL;
  uint8_t *encodedValue = NULL;
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;

  if ((iovarName == NULL) || (logName == NULL))
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_ReserveControlCredit("iovar", iovarName, &availableCredits) == 0U)
  {
    APP_WiFi_Logf("[wifi] iovar: no credits available for %s tx=%u credit=%u\n",
                  iovarName,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], APP_WIFI_WLC_SET_VAR);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           APP_WIFI_SDPCM_IOCTL_SET);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  payload = &frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE];
  memcpy(payload, iovarName, nameLength);
  bssIndex = &payload[nameLength];
  encodedValue = &bssIndex[sizeof(uint32_t)];
  APP_WiFi_WriteLe32(bssIndex, APP_WIFI_STA_BSSCFG_INDEX);
  APP_WiFi_WriteLe32(encodedValue, (uint32_t)value);

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] iovar: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  logName,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiPendingIovarRequest = requestType;
  g_wifiLastIoctlCommand = APP_WIFI_WLC_SET_VAR;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = (uint32_t)value;
  APP_WiFi_Logf("[wifi] iovar: sent %s id=%u txseq=%u credits=%u bss=%lu value=%ld\n",
                logName,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                (unsigned long)APP_WIFI_STA_BSSCFG_INDEX,
                (long)value);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendSetWsecIoctl(uint32_t wsec)
{
  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_SET_WSEC,
                                   &wsec,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_SET_WSEC");
}

static HAL_StatusTypeDef APP_WiFi_SendSetInfraIoctl(uint32_t infraMode)
{
  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_SET_INFRA,
                                   &infraMode,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_SET_INFRA");
}

static HAL_StatusTypeDef APP_WiFi_SendSetAuthIoctl(uint32_t authMode)
{
  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_SET_AUTH,
                                   &authMode,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_SET_AUTH");
}

static HAL_StatusTypeDef APP_WiFi_SendSetWpaAuthIoctl(uint32_t wpaAuth)
{
  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_SET_WPA_AUTH,
                                   &wpaAuth,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_SET_WPA_AUTH");
}

static HAL_StatusTypeDef APP_WiFi_SendGetBssidIoctl(void)
{
  uint8_t bssid[APP_WIFI_MAC_ADDRESS_SIZE] = {0U};

  return APP_WiFi_SendBufferedControlIoctl(APP_WIFI_SDPCM_IOCTL_GET,
                                           APP_WIFI_WLC_GET_BSSID,
                                           bssid,
                                           (uint16_t)sizeof(bssid),
                                           "WLC_GET_BSSID");
}

static HAL_StatusTypeDef APP_WiFi_SendSetSupWpaIovar(uint32_t enabled)
{
  return APP_WiFi_SendSetBsscfgU32Iovar(APP_WIFI_IOVAR_BSSCFG_SUP_WPA,
                                        (int32_t)enabled,
                                        APP_WIFI_IOVAR_BSSCFG_SUP_WPA,
                                        APP_WIFI_IOVAR_REQUEST_SUP_WPA_SET);
}

static HAL_StatusTypeDef APP_WiFi_SendSetSupWpa2EapverIovar(int32_t eapVersion)
{
  return APP_WiFi_SendSetBsscfgU32Iovar(APP_WIFI_IOVAR_BSSCFG_SUP_WPA2_EAPVER,
                                        eapVersion,
                                        APP_WIFI_IOVAR_BSSCFG_SUP_WPA2_EAPVER,
                                        APP_WIFI_IOVAR_REQUEST_SUP_WPA2_EAPVER_SET);
}

static HAL_StatusTypeDef APP_WiFi_SendSetSupWpaTimeoutIovar(int32_t timeoutMs)
{
  return APP_WiFi_SendSetBsscfgU32Iovar(APP_WIFI_IOVAR_BSSCFG_SUP_WPA_TMO,
                                        timeoutMs,
                                        APP_WIFI_IOVAR_BSSCFG_SUP_WPA_TMO,
                                        APP_WIFI_IOVAR_REQUEST_SUP_WPA_TMO_SET);
}

static HAL_StatusTypeDef APP_WiFi_SendSetPassphrasePmk(const uint8_t *passphrase, uint16_t passphraseLength)
{
  APP_WiFi_Pmk_t pmk;

  if ((passphrase == NULL) ||
      (passphraseLength < APP_WIFI_WSEC_MIN_PSK_LEN) ||
      (passphraseLength > APP_WIFI_WSEC_MAX_PSK_LEN))
  {
    return HAL_ERROR;
  }

  memset(&pmk, 0, sizeof(pmk));
  pmk.key_len = passphraseLength;
  pmk.flags = APP_WIFI_WSEC_PASSPHRASE;
  memcpy(pmk.key, passphrase, passphraseLength);

  /*
   * Match the vendor WICED flow for 43362-class chips: the radio firmware
   * needs a short settle window before it reliably accepts the PMK ioctl.
   */
  osDelay(1U);

  return APP_WiFi_SendBufferedControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                           APP_WIFI_WLC_SET_WSEC_PMK,
                                           &pmk,
                                           (uint16_t)sizeof(pmk),
                                           "WLC_SET_WSEC_PMK");
}

static HAL_StatusTypeDef APP_WiFi_SendJoinSsidIoctl(void)
{
  APP_WiFi_Ssid_t ssid;

  memset(&ssid, 0, sizeof(ssid));
  ssid.SSID_len = g_wifiJoinRequestedSsidLength;
  memcpy(ssid.SSID, g_wifiJoinRequestedSsid, g_wifiJoinRequestedSsidLength);

  return APP_WiFi_SendBufferedControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                           APP_WIFI_WLC_SET_SSID,
                                           &ssid,
                                           (uint16_t)sizeof(ssid),
                                           "WLC_SET_SSID");
}

static HAL_StatusTypeDef APP_WiFi_SendSetPmOffIoctl(void)
{
  const uint32_t pmValue = APP_WIFI_PM_OFF;

  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_SET_PM,
                                   &pmValue,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_SET_PM(PM_OFF)");
}

static HAL_StatusTypeDef APP_WiFi_SendClearScanSuppressIoctl(void)
{
  const uint32_t scanSuppress = 0U;

  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_SET_SCANSUPPRESS,
                                   &scanSuppress,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_SET_SCANSUPPRESS(0)");
}

static HAL_StatusTypeDef APP_WiFi_SendEscanIovar(void)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + 128U] = {0};
  const char *iovarName = APP_WIFI_IOVAR_ESCAN;
  const uint16_t nameLength = (uint16_t)(strlen(iovarName) + 1U);
  const uint16_t escanParamsLength = (uint16_t)(offsetof(APP_WiFi_EscanParams_t, params) + 64U);
  const uint16_t payloadLength = (uint16_t)(nameLength + escanParamsLength);
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);
  APP_WiFi_EscanParams_t *scanParams = NULL;
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;

  if (APP_WiFi_ReserveControlCredit("scan", APP_WIFI_IOVAR_ESCAN, &availableCredits) == 0U)
  {
    APP_WiFi_Logf("[wifi] scan: no credits available for %s tx=%u credit=%u\n",
                  APP_WIFI_IOVAR_ESCAN,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], APP_WIFI_WLC_SET_VAR);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           APP_WIFI_SDPCM_IOCTL_SET);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  memcpy(&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
         iovarName,
         nameLength);

  scanParams = (APP_WiFi_EscanParams_t *)&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + nameLength];
  memset(scanParams, 0, escanParamsLength);
  scanParams->version = APP_WIFI_ESCAN_REQ_VERSION;
  scanParams->action = APP_WIFI_WL_SCAN_ACTION_START;
  scanParams->sync_id = APP_WIFI_ESCAN_SYNC_ID;
  memset(scanParams->params.bssid.octet, 0xFF, sizeof(scanParams->params.bssid.octet));
  scanParams->params.bss_type = APP_WIFI_WL_BSSTYPE_ANY;
  scanParams->params.scan_type = 0;
  scanParams->params.nprobes = -1;
  scanParams->params.active_time = -1;
  scanParams->params.passive_time = -1;
  scanParams->params.home_time = -1;
  scanParams->params.channel_num = 0;

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] scan: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  APP_WIFI_IOVAR_ESCAN,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_ESCAN_SET;
  g_wifiLastIoctlCommand = APP_WIFI_WLC_SET_VAR;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = 0U;
  APP_WiFi_ClearCachedScanResults();
  g_wifiScanAborted = 0U;
  g_wifiScanCompleted = 0U;
  g_wifiScanPartialCount = 0U;
  g_wifiScanResultCount = 0U;
  APP_WiFi_Logf("[wifi] scan: sent %s id=%u txseq=%u credits=%u sync=%u\n",
                APP_WIFI_IOVAR_ESCAN,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                (unsigned int)APP_WIFI_ESCAN_SYNC_ID);
  return HAL_OK;
}

APP_WiFiTxStatus_t APP_WiFi_SendDataFrame(const uint8_t *frame, uint16_t length)
{
  static uint32_t dataTxAttemptCount = 0U;
  static uint32_t dataTxOkCount = 0U;
  static uint32_t dataTxFailCount = 0U;
  static uint32_t dataTxBusyCount = 0U;
  uint8_t txFrame[APP_WIFI_SDPCM_HEADER_SIZE +
                  APP_WIFI_SDPCM_DATA_PADDING_SIZE +
                  APP_WIFI_SDPCM_BDC_HEADER_SIZE +
                  APP_WIFI_LWIP_TX_PACKET_MAX_LEN] = {0U};
  const uint16_t ethernetLength = (length < APP_WIFI_ETHERNET_MIN_FRAME_LEN) ?
                                  APP_WIFI_ETHERNET_MIN_FRAME_LEN :
                                  length;
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE +
                                          APP_WIFI_SDPCM_DATA_PADDING_SIZE +
                                          APP_WIFI_SDPCM_BDC_HEADER_SIZE +
                                          ethernetLength);
  const uint16_t payloadOffset = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE +
                                            APP_WIFI_SDPCM_DATA_PADDING_SIZE +
                                            APP_WIFI_SDPCM_BDC_HEADER_SIZE);
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;

  if ((frame == NULL) || (length == 0U) || (length > APP_WIFI_LWIP_TX_PACKET_MAX_LEN))
  {
    return APP_WIFI_TX_STATUS_ERROR;
  }

  if (frameLength > APP_WIFI_SDPCM_MAX_FRAME_LEN)
  {
    return APP_WIFI_TX_STATUS_ERROR;
  }

  availableCredits = (uint8_t)(g_wifiBusCredit - g_wifiTxSequence);
  if (availableCredits == 0U)
  {
    dataTxBusyCount++;
      if ((APP_WIFI_DATA_TX_LOG_ENABLE != 0U) && (dataTxBusyCount <= 32U))
      {
        APP_WiFi_Logf("[wifi] data tx busy #%lu len=%u txseq=%u credit=%u flow=%u\n",
                    (unsigned long)dataTxBusyCount,
                    (unsigned int)length,
                    (unsigned int)g_wifiTxSequence,
                    (unsigned int)g_wifiBusCredit,
                    (unsigned int)g_wifiLastSdpcmFlowControl);
    }
    return APP_WIFI_TX_STATUS_BUSY;
  }

  APP_WiFi_WriteLe16(&txFrame[0], frameLength);
  APP_WiFi_WriteLe16(&txFrame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  txFrame[4] = txSequence;
  txFrame[5] = APP_WIFI_SDPCM_CHANNEL_DATA;
  txFrame[7] = (uint8_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_DATA_PADDING_SIZE);

  txFrame[14] = (uint8_t)(APP_WIFI_BDC_PROTO_VER << APP_WIFI_BDC_FLAG_VER_SHIFT);
  txFrame[15] = 0U;
  txFrame[16] = (uint8_t)APP_WIFI_STA_BSSCFG_INDEX;
  txFrame[17] = 0U;

  memcpy(&txFrame[payloadOffset], frame, length);
  if (ethernetLength > length)
  {
    memset(&txFrame[payloadOffset + length], 0, (size_t)(ethernetLength - length));
  }

  dataTxAttemptCount++;
  if ((APP_WIFI_DATA_TX_LOG_ENABLE != 0U) && (dataTxAttemptCount <= 32U))
  {
    APP_WiFi_Logf("[wifi] data tx try #%lu len=%u frameLen=%u txseq=%u credit=%u eth=0x%02X%02X\n",
                  (unsigned long)dataTxAttemptCount,
                  (unsigned int)length,
                  (unsigned int)frameLength,
                  (unsigned int)txSequence,
                  (unsigned int)g_wifiBusCredit,
                  (length >= 13U) ? (unsigned int)frame[12] : 0U,
                  (length >= 14U) ? (unsigned int)frame[13] : 0U);
  }

  if (APP_WiFi_Platform_Fn2Write(txFrame, frameLength) != HAL_OK)
  {
    dataTxFailCount++;
    if ((APP_WIFI_DATA_TX_LOG_ENABLE != 0U) && (dataTxFailCount <= 32U))
    {
      APP_WiFi_Logf("[wifi] data tx failed #%lu len=%u sta=0x%08lX err=0x%08lX\n",
                    (unsigned long)dataTxFailCount,
                    (unsigned int)length,
                    (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    }
    return APP_WIFI_TX_STATUS_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  dataTxOkCount++;
  if ((APP_WIFI_DATA_TX_LOG_ENABLE != 0U) && (dataTxOkCount <= 32U))
  {
    APP_WiFi_Logf("[wifi] data tx ok #%lu len=%u txseq=%u\n",
                  (unsigned long)dataTxOkCount,
                  (unsigned int)length,
                  (unsigned int)txSequence);
  }
  return APP_WIFI_TX_STATUS_OK;
}

static void APP_WiFi_HandleAsyncEvent(const uint8_t *frame, uint16_t captured)
{
  static const uint8_t brcmOui[APP_WIFI_BRCM_EVENT_OUI_LEN] = {0x00U, 0x10U, 0x18U};
  uint8_t sdpcmHeaderLength = 0U;
  const APP_WiFi_BdcHeader_t *bdcHeader = NULL;
  const APP_WiFi_BcmEvent_t *event = NULL;
  uint16_t eventOffset = 0U;
  uint16_t eventDataOffset = 0U;
  uint16_t availableEventData = 0U;
  uint16_t scanSyncId = 0U;
  uint16_t scanBssCount = 0U;
  uint16_t eventFlags = 0U;
  uint32_t eventType = 0U;
  uint32_t status = 0U;
  uint32_t reason = 0U;
  uint32_t dataLength = 0U;

  if (frame == NULL)
  {
    return;
  }

  sdpcmHeaderLength = frame[7];
  if (captured < (uint16_t)(sdpcmHeaderLength + sizeof(APP_WiFi_BdcHeader_t)))
  {
    return;
  }

  bdcHeader = (const APP_WiFi_BdcHeader_t *)&frame[sdpcmHeaderLength];
  eventOffset = (uint16_t)(sdpcmHeaderLength + (uint16_t)((bdcHeader->data_offset + 1U) * APP_WIFI_SDPCM_BDC_HEADER_SIZE));
  if (captured < (uint16_t)(eventOffset + sizeof(APP_WiFi_BcmEvent_t)))
  {
    return;
  }

  event = (const APP_WiFi_BcmEvent_t *)&frame[eventOffset];
  if (APP_WiFi_ReadBe16((const uint8_t *)&event->ether.ethertype) != APP_WIFI_BRCM_ETHER_TYPE)
  {
    return;
  }

  if (memcmp(event->bcmeth.oui, brcmOui, sizeof(brcmOui)) != 0)
  {
    return;
  }

  eventType = APP_WiFi_ReadBe32((const uint8_t *)&event->raw.event_type);
  eventFlags = APP_WiFi_ReadBe16((const uint8_t *)&event->raw.flags);
  status = APP_WiFi_ReadBe32((const uint8_t *)&event->raw.status);
  reason = APP_WiFi_ReadBe32((const uint8_t *)&event->raw.reason);
  dataLength = APP_WiFi_ReadBe32((const uint8_t *)&event->raw.datalen);
  eventDataOffset = (uint16_t)(eventOffset + sizeof(APP_WiFi_BcmEvent_t));
  availableEventData = (captured > eventDataOffset) ? (uint16_t)(captured - eventDataOffset) : 0U;

  switch (eventType)
  {
    case APP_WIFI_WLC_E_SET_SSID:
      g_wifiLastJoinEventType = eventType;
      g_wifiLastJoinEventStatus = status;
      g_wifiLastJoinEventReason = reason;
      APP_WiFi_Logf("[wifi] join: set_ssid status=%lu reason=%lu\n",
                    (unsigned long)status,
                    (unsigned long)reason);
      if (status == APP_WIFI_WLC_E_STATUS_SUCCESS)
      {
        g_wifiJoinSsidSet = 1U;
        APP_WiFi_EvaluateJoinCompletion();
      }
      else if (status == APP_WIFI_WLC_E_STATUS_NO_NETWORKS)
      {
        APP_WiFi_FailJoin("no matching network found", status, reason);
      }
      else if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING)
      {
        APP_WiFi_FailJoin("set_ssid failed", status, reason);
      }
      break;

    case APP_WIFI_WLC_E_JOIN:
    case APP_WIFI_WLC_E_ASSOC:
      APP_WiFi_Logf("[wifi] join: event=%lu status=%lu reason=%lu\n",
                    (unsigned long)eventType,
                    (unsigned long)status,
                    (unsigned long)reason);
      break;

    case APP_WIFI_WLC_E_AUTH:
      g_wifiLastJoinEventType = eventType;
      g_wifiLastJoinEventStatus = status;
      g_wifiLastJoinEventReason = reason;
      APP_WiFi_Logf("[wifi] join: auth status=%lu reason=%lu authType=%lu\n",
                    (unsigned long)status,
                    (unsigned long)reason,
                    (unsigned long)APP_WiFi_ReadBe32((const uint8_t *)&event->raw.auth_type));
      if (status == APP_WIFI_WLC_E_STATUS_SUCCESS)
      {
        g_wifiJoinAuthenticated = 1U;
        if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTED)
        {
          APP_WiFi_ClearConnectedLinkIssue("auth");
        }
        APP_WiFi_EvaluateJoinCompletion();
      }
      else if ((status != APP_WIFI_WLC_E_STATUS_UNSOLICITED) &&
               (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING))
      {
        APP_WiFi_FailJoin("authentication failed", status, reason);
      }
      break;

    case APP_WIFI_WLC_E_LINK:
    {
      const uint8_t linkUp = ((eventFlags & APP_WIFI_WLC_EVENT_MSG_LINK) != 0U) ? 1U : 0U;

      g_wifiLastJoinEventType = eventType;
      g_wifiLastJoinEventStatus = status;
      g_wifiLastJoinEventReason = reason;
      APP_WiFi_Logf("[wifi] join: link %s status=%lu reason=%lu flags=0x%04X\n",
                    (linkUp != 0U) ? "up" : "down",
                    (unsigned long)status,
                    (unsigned long)reason,
                    (unsigned int)eventFlags);
      if (linkUp != 0U)
      {
        g_wifiJoinLinkReady = 1U;
        APP_WiFi_ClearConnectedLinkIssue("link up");
        APP_WiFi_EvaluateJoinCompletion();
      }
      else if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTED)
      {
        APP_WiFi_MarkConnectedLinkIssue("link down", status, reason);
        APP_WiFi_Logf("[wifi] join: transient link down deferred while connected\n");
      }
      else
      {
        g_wifiJoinLinkReady = 0U;
        if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING)
        {
          APP_WiFi_FailJoin("link dropped", status, reason);
        }
      }
      break;
    }

    case APP_WIFI_WLC_E_DEAUTH_IND:
    case APP_WIFI_WLC_E_DISASSOC_IND:
      g_wifiLastJoinEventType = eventType;
      g_wifiLastJoinEventStatus = status;
      g_wifiLastJoinEventReason = reason;
      APP_WiFi_Logf("[wifi] join: %s status=%lu reason=%lu\n",
                    (eventType == APP_WIFI_WLC_E_DEAUTH_IND) ? "deauth" : "disassoc",
                    (unsigned long)status,
                    (unsigned long)reason);
      if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING)
      {
        g_wifiJoinAuthenticated = 0U;
        g_wifiJoinLinkReady = 0U;
        APP_WiFi_FailJoin((eventType == APP_WIFI_WLC_E_DEAUTH_IND) ? "deauthenticated" : "disassociated",
                          status,
                          reason);
      }
      else if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTED)
      {
        APP_WiFi_MarkConnectedLinkIssue((eventType == APP_WIFI_WLC_E_DEAUTH_IND) ? "deauth" : "disassoc",
                                        status,
                                        reason);
        APP_WiFi_Logf("[wifi] join: %s deferred while connected\n",
                      (eventType == APP_WIFI_WLC_E_DEAUTH_IND) ? "deauth" : "disassoc");
      }
      else
      {
        g_wifiJoinAuthenticated = 0U;
        g_wifiJoinLinkReady = 0U;
      }
      break;

    case APP_WIFI_WLC_E_PSK_SUP:
      g_wifiLastJoinEventType = eventType;
      g_wifiLastJoinEventStatus = status;
      g_wifiLastJoinEventReason = reason;
      APP_WiFi_Logf("[wifi] join: psk_sup status=%lu reason=%lu\n",
                    (unsigned long)status,
                    (unsigned long)reason);
      if (status == APP_WIFI_WLC_SUP_KEYED)
      {
        g_wifiJoinSecurityComplete = 1U;
        if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTED)
        {
          APP_WiFi_ClearConnectedLinkIssue("psk keyed");
        }
        APP_WiFi_EvaluateJoinCompletion();
      }
      else if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING)
      {
        APP_WiFi_FailJoin("wpa handshake failed", status, reason);
      }
      break;

    case APP_WIFI_WLC_E_PRUNE:
      APP_WiFi_Logf("[wifi] join: prune status=%lu reason=%lu\n",
                    (unsigned long)status,
                    (unsigned long)reason);
      if (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING)
      {
        APP_WiFi_FailJoin("ap pruned from join candidates", status, reason);
      }
      break;

    default:
      break;
  }

  if (eventType == APP_WIFI_WLC_E_SCAN_COMPLETE)
  {
    APP_WiFi_Logf("[wifi] scan: scan_complete status=%lu reason=%lu\n",
                  (unsigned long)status,
                  (unsigned long)reason);
    if ((g_wifiScanIoctlCompleted != 0U) &&
        (g_wifiScanCompleted == 0U) &&
        (g_wifiScanAborted == 0U))
    {
      if (status == APP_WIFI_WLC_E_STATUS_SUCCESS)
      {
        g_wifiScanCompleted = 1U;
      }
      else
      {
        g_wifiScanAborted = 1U;
      }
    }
    return;
  }

  if (eventType != APP_WIFI_WLC_E_ESCAN_RESULT)
  {
    return;
  }

  if (availableEventData >= sizeof(APP_WiFi_EscanResult_t))
  {
    const APP_WiFi_EscanResult_t *scanResult = (const APP_WiFi_EscanResult_t *)&frame[eventDataOffset];
    scanSyncId = APP_WiFi_ReadLe16((const uint8_t *)&scanResult->sync_id);
    scanBssCount = APP_WiFi_ReadLe16((const uint8_t *)&scanResult->bss_count);
    g_wifiLastScanSyncId = scanSyncId;
    g_wifiLastScanBssCount = scanBssCount;
  }

  if (status == APP_WIFI_WLC_E_STATUS_PARTIAL)
  {
    g_wifiScanPartialCount++;
    g_wifiScanResultCount += scanBssCount;

    if (availableEventData >= (uint16_t)(sizeof(APP_WiFi_EscanResult_t) + sizeof(APP_WiFi_BssInfo_t)))
    {
      APP_WiFi_BssInfo_t bssInfo;

      memcpy(&bssInfo,
             &frame[eventDataOffset + sizeof(APP_WiFi_EscanResult_t)],
             sizeof(bssInfo));

      if (bssInfo.version == APP_WIFI_WL_BSS_INFO_VERSION)
      {
        APP_WiFi_UpdateCachedScanResult(&bssInfo);
        APP_WiFi_CopyScanSsid(g_wifiLastScanSsid, bssInfo.ssid, bssInfo.ssid_length);
        g_wifiLastScanRssi = bssInfo.rssi;
        g_wifiLastScanChannel = (bssInfo.ctl_ch != 0U) ? bssInfo.ctl_ch : (uint8_t)(bssInfo.chanspec & 0xFFU);

        APP_WiFi_Logf("[wifi] scan: partial sync=%u bss=%u ssid=\"%s\" rssi=%d ch=%u\n",
                      (unsigned int)scanSyncId,
                      (unsigned int)scanBssCount,
                      g_wifiLastScanSsid,
                      (int)g_wifiLastScanRssi,
                      (unsigned int)g_wifiLastScanChannel);
        return;
      }
    }

    APP_WiFi_Logf("[wifi] scan: partial sync=%u bss=%u dataLen=%lu captured=%u\n",
                  (unsigned int)scanSyncId,
                  (unsigned int)scanBssCount,
                  (unsigned long)dataLength,
                  (unsigned int)availableEventData);
    return;
  }

  if ((status == APP_WIFI_WLC_E_STATUS_SUCCESS) ||
      (status == APP_WIFI_WLC_E_STATUS_NO_NETWORKS))
  {
    g_wifiScanCompleted = 1U;
    APP_WiFi_Logf("[wifi] scan: complete sync=%u status=%lu results=%lu partial=%lu\n",
                  (unsigned int)scanSyncId,
                  (unsigned long)status,
                  (unsigned long)g_wifiScanResultCount,
                  (unsigned long)g_wifiScanPartialCount);
    return;
  }

  if ((status == APP_WIFI_WLC_E_STATUS_ABORT) ||
      (status == APP_WIFI_WLC_E_STATUS_NEWSCAN) ||
      (status == APP_WIFI_WLC_E_STATUS_NEWASSOC))
  {
    g_wifiScanAborted = 1U;
    APP_WiFi_Logf("[wifi] scan: aborted sync=%u status=%lu reason=%lu\n",
                  (unsigned int)scanSyncId,
                  (unsigned long)status,
                  (unsigned long)reason);
    return;
  }

  APP_WiFi_Logf("[wifi] scan: event sync=%u status=%lu reason=%lu dataLen=%lu\n",
                (unsigned int)scanSyncId,
                (unsigned long)status,
                (unsigned long)reason,
                (unsigned long)dataLength);
}

static void APP_WiFi_ResetJoinProgress(void)
{
  g_wifiJoinStep = APP_WIFI_JOIN_STEP_IDLE;
  g_wifiJoinSsidSet = 0U;
  g_wifiJoinAuthenticated = 0U;
  g_wifiJoinLinkReady = 0U;
  g_wifiJoinSecurityComplete = 0U;
  g_wifiJoinFailureStatus = 0U;
  g_wifiJoinFailureReason = 0U;
  g_wifiLastJoinEventType = 0U;
  g_wifiLastJoinEventStatus = 0U;
  g_wifiLastJoinEventReason = 0U;
  g_wifiJoinStartTick = 0U;
  g_wifiJoinLastPollTick = 0U;
  g_wifiConnectedLinkDownPending = 0U;
  g_wifiConnectedLinkDownTick = 0U;
  g_wifiConnectedLinkDownStatus = 0U;
  g_wifiConnectedLinkDownReason = 0U;
  g_wifiConnectedLinkDownFrameCount = 0U;
  memset(g_wifiLastBssid, 0, sizeof(g_wifiLastBssid));
}

static void APP_WiFi_StartJoinAttempt(uint32_t now)
{
  APP_WiFi_ResetJoinProgress();
  g_wifiLinkState = APP_WIFI_LINK_STATE_CONNECTING;
  g_wifiJoinStartTick = now;
  g_wifiJoinLastPollTick = 0U;

  if (g_wifiJoinSecurity == APP_WIFI_JOIN_SECURITY_OPEN)
  {
    g_wifiJoinAuthenticated = 1U;
    g_wifiJoinSecurityComplete = 1U;
  }

  g_wifiJoinStep = APP_WIFI_JOIN_STEP_SEND_WSEC;
}

static void APP_WiFi_ProcessJoinRetry(void)
{
  const uint32_t now = HAL_GetTick();

  if ((g_wifiState != APP_WIFI_STATE_MAILBOX_READY) ||
      (g_wifiLinkState != APP_WIFI_LINK_STATE_FAILED) ||
      (g_wifiJoinRetryAtTick == 0U) ||
      (g_wifiJoinRequestedSsidLength == 0U))
  {
    return;
  }

  if ((int32_t)(now - g_wifiJoinRetryAtTick) < 0)
  {
    return;
  }

  APP_WiFi_Logf("[wifi] join: retry %u/%u ssid=\"%s\"\n",
                (unsigned int)g_wifiJoinRetryCount,
                (unsigned int)APP_WIFI_JOIN_RETRY_MAX,
                g_wifiJoinRequestedSsid);
  g_wifiJoinRetryAtTick = 0U;
  APP_WiFi_StartJoinAttempt(now);
}

static void APP_WiFi_EvaluateJoinCompletion(void)
{
  if (g_wifiLinkState != APP_WIFI_LINK_STATE_CONNECTING)
  {
    return;
  }

  if (g_wifiJoinSsidSet == 0U)
  {
    return;
  }

  if (g_wifiJoinLinkReady == 0U)
  {
    return;
  }

  if ((g_wifiJoinSecurity == APP_WIFI_JOIN_SECURITY_WPA2_PSK) &&
      (g_wifiJoinSecurityComplete == 0U))
  {
    return;
  }

  g_wifiLinkState = APP_WIFI_LINK_STATE_CONNECTED;
  g_wifiJoinStep = APP_WIFI_JOIN_STEP_IDLE;
  g_wifiJoinRetryAtTick = 0U;
  g_wifiJoinRetryCount = 0U;
  APP_WiFi_Logf("[wifi] join: connected ssid=\"%s\" auth=%u link=%u security=%u\n",
                g_wifiJoinRequestedSsid,
                (unsigned int)g_wifiJoinAuthenticated,
                (unsigned int)g_wifiJoinLinkReady,
                (unsigned int)g_wifiJoinSecurityComplete);
}

static void APP_WiFi_MarkConnectedLinkIssue(const char *source, uint32_t status, uint32_t reason)
{
  if (g_wifiLinkState != APP_WIFI_LINK_STATE_CONNECTED)
  {
    return;
  }

  if (g_wifiConnectedLinkDownPending == 0U)
  {
    g_wifiConnectedLinkDownPending = 1U;
    g_wifiConnectedLinkDownTick = HAL_GetTick();
    g_wifiConnectedLinkDownFrameCount = g_wifiSdpcmFrameCount;
    APP_WiFi_Logf("[wifi] runtime: link issue from %s, grace=%lu ms\n",
                  (source != NULL) ? source : "unknown",
                  (unsigned long)APP_WIFI_CONNECTED_LINK_DOWN_GRACE_MS);
  }

  g_wifiConnectedLinkDownStatus = status;
  g_wifiConnectedLinkDownReason = reason;
}

static void APP_WiFi_ClearConnectedLinkIssue(const char *source)
{
  if (g_wifiConnectedLinkDownPending != 0U)
  {
    APP_WiFi_Logf("[wifi] runtime: link restored by %s after %lu ms\n",
                  (source != NULL) ? source : "event",
                  (unsigned long)(HAL_GetTick() - g_wifiConnectedLinkDownTick));
  }

  g_wifiConnectedLinkDownPending = 0U;
  g_wifiConnectedLinkDownTick = 0U;
  g_wifiConnectedLinkDownStatus = 0U;
  g_wifiConnectedLinkDownReason = 0U;
  g_wifiConnectedLinkDownFrameCount = 0U;
}

static void APP_WiFi_ProcessConnectedLinkRecovery(void)
{
  const uint32_t now = HAL_GetTick();

  if (g_wifiLinkState != APP_WIFI_LINK_STATE_CONNECTED)
  {
    g_wifiConnectedLinkDownPending = 0U;
    g_wifiConnectedLinkDownTick = 0U;
    g_wifiConnectedLinkDownStatus = 0U;
    g_wifiConnectedLinkDownReason = 0U;
    g_wifiConnectedLinkDownFrameCount = 0U;
    return;
  }

  if (g_wifiConnectedLinkDownPending == 0U)
  {
    return;
  }

  if (g_wifiSdpcmFrameCount != g_wifiConnectedLinkDownFrameCount)
  {
    APP_WiFi_ClearConnectedLinkIssue("rx traffic");
    return;
  }

  if ((now - g_wifiConnectedLinkDownTick) < APP_WIFI_CONNECTED_LINK_DOWN_GRACE_MS)
  {
    return;
  }

  g_wifiJoinAuthenticated = 0U;
  g_wifiJoinLinkReady = 0U;
  g_wifiJoinSecurityComplete = 0U;
  APP_WiFi_Logf("[wifi] runtime: link issue persisted, reconnect status=%lu reason=%lu\n",
                (unsigned long)g_wifiConnectedLinkDownStatus,
                (unsigned long)g_wifiConnectedLinkDownReason);
  APP_WiFi_FailJoin("runtime link lost", g_wifiConnectedLinkDownStatus, g_wifiConnectedLinkDownReason);
}

static void APP_WiFi_FailJoin(const char *reason, uint32_t status, uint32_t detail)
{
  if (g_wifiLinkState == APP_WIFI_LINK_STATE_FAILED)
  {
    return;
  }

  g_wifiLinkState = APP_WIFI_LINK_STATE_FAILED;
  g_wifiJoinFailureStatus = status;
  g_wifiJoinFailureReason = detail;
  g_wifiJoinStep = APP_WIFI_JOIN_STEP_IDLE;
  g_wifiJoinAuthenticated = 0U;
  g_wifiJoinLinkReady = 0U;
  g_wifiJoinSecurityComplete = 0U;
  g_wifiConnectedLinkDownPending = 0U;
  g_wifiConnectedLinkDownTick = 0U;
  g_wifiConnectedLinkDownStatus = 0U;
  g_wifiConnectedLinkDownReason = 0U;
  g_wifiConnectedLinkDownFrameCount = 0U;
  if ((g_wifiJoinRequestedSsidLength != 0U) &&
      (g_wifiJoinRetryCount < APP_WIFI_JOIN_RETRY_MAX))
  {
    g_wifiJoinRetryCount++;
    g_wifiJoinRetryAtTick = HAL_GetTick() + APP_WIFI_JOIN_RETRY_DELAY_MS;
  }
  else
  {
    g_wifiJoinRetryAtTick = 0U;
  }
  APP_WiFi_Logf("[wifi] join: failed ssid=\"%s\" reason=\"%s\" status=%lu detail=%lu\n",
                g_wifiJoinRequestedSsid,
                (reason != NULL) ? reason : "unknown",
                (unsigned long)status,
                (unsigned long)detail);
  if (g_wifiJoinRetryAtTick != 0U)
  {
    APP_WiFi_Logf("[wifi] join: retry scheduled in %lu ms (%u/%u)\n",
                  (unsigned long)APP_WIFI_JOIN_RETRY_DELAY_MS,
                  (unsigned int)g_wifiJoinRetryCount,
                  (unsigned int)APP_WIFI_JOIN_RETRY_MAX);
  }
}

static void APP_WiFi_ProcessJoinRequest(void)
{
  const uint32_t now = HAL_GetTick();

  if ((g_wifiState != APP_WIFI_STATE_MAILBOX_READY) ||
      (g_wifiLinkState != APP_WIFI_LINK_STATE_CONNECTING))
  {
    return;
  }

  switch (g_wifiJoinStep)
  {
    case APP_WIFI_JOIN_STEP_SEND_WSEC:
    {
      const HAL_StatusTypeDef status = APP_WiFi_SendSetWsecIoctl((g_wifiJoinSecurity == APP_WIFI_JOIN_SECURITY_OPEN) ? 0UL : APP_WIFI_WSEC_AES);
      if (status == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_WSEC;
      }
      else if (status == HAL_ERROR)
      {
        (void)APP_WiFi_Platform_AbortFunction2Read();
        APP_WiFi_FailJoin("send wsec failed", APP_WiFi_Platform_GetLastSdioStatus(), APP_WiFi_Platform_GetLastSdioError());
      }
      break;
    }

    case APP_WIFI_JOIN_STEP_WAIT_WSEC:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_WSEC))
      {
        if (g_wifiLastIoctlStatus == 0U)
        {
          g_wifiJoinStep = (g_wifiJoinSecurity == APP_WIFI_JOIN_SECURITY_OPEN) ?
                           APP_WIFI_JOIN_STEP_SEND_INFRA :
                           APP_WIFI_JOIN_STEP_SEND_SUP_WPA;
        }
        else
        {
          APP_WiFi_FailJoin("set wsec failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_SEND_SUP_WPA:
      if (APP_WiFi_SendSetSupWpaIovar((g_wifiJoinSecurity == APP_WIFI_JOIN_SECURITY_OPEN) ? 0UL : 1UL) == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_SUP_WPA;
      }
      break;

    case APP_WIFI_JOIN_STEP_WAIT_SUP_WPA:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_VAR))
      {
        if ((g_wifiLastIoctlStatus == 0U) || (g_wifiLastIoctlStatus == APP_WIFI_BCME_UNSUPPORTED))
        {
          if (g_wifiLastIoctlStatus == APP_WIFI_BCME_UNSUPPORTED)
          {
            APP_WiFi_Logf("[wifi] iovar: %s unsupported, continuing\n",
                          APP_WIFI_IOVAR_BSSCFG_SUP_WPA);
          }
          g_wifiJoinStep = APP_WIFI_JOIN_STEP_SEND_SUP_WPA2_EAPVER;
        }
        else
        {
          APP_WiFi_FailJoin("set sup_wpa failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_SEND_SUP_WPA2_EAPVER:
      if (APP_WiFi_SendSetSupWpa2EapverIovar(-1) == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_SUP_WPA2_EAPVER;
      }
      break;

    case APP_WIFI_JOIN_STEP_WAIT_SUP_WPA2_EAPVER:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_VAR))
      {
        if ((g_wifiLastIoctlStatus == 0U) || (g_wifiLastIoctlStatus == APP_WIFI_BCME_UNSUPPORTED))
        {
          if (g_wifiLastIoctlStatus == APP_WIFI_BCME_UNSUPPORTED)
          {
            APP_WiFi_Logf("[wifi] iovar: %s unsupported, continuing\n",
                          APP_WIFI_IOVAR_BSSCFG_SUP_WPA2_EAPVER);
          }
          g_wifiJoinStep = (g_wifiJoinSecurity == APP_WIFI_JOIN_SECURITY_OPEN) ?
                           APP_WIFI_JOIN_STEP_SEND_INFRA :
                           APP_WIFI_JOIN_STEP_SEND_SUP_WPA_TMO;
        }
        else
        {
          APP_WiFi_FailJoin("set sup_wpa2_eapver failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_SEND_SUP_WPA_TMO:
      if (APP_WiFi_SendSetSupWpaTimeoutIovar(APP_WIFI_EAPOL_KEY_TIMEOUT_MS) == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_SUP_WPA_TMO;
      }
      break;

    case APP_WIFI_JOIN_STEP_WAIT_SUP_WPA_TMO:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_VAR))
      {
        if ((g_wifiLastIoctlStatus == 0U) || (g_wifiLastIoctlStatus == APP_WIFI_BCME_UNSUPPORTED))
        {
          if (g_wifiLastIoctlStatus == APP_WIFI_BCME_UNSUPPORTED)
          {
            APP_WiFi_Logf("[wifi] iovar: %s unsupported, continuing\n",
                          APP_WIFI_IOVAR_BSSCFG_SUP_WPA_TMO);
          }
          g_wifiJoinStep = APP_WIFI_JOIN_STEP_SEND_PMK;
        }
        else
        {
          APP_WiFi_FailJoin("set sup_wpa_tmo failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_SEND_PMK:
    {
      const HAL_StatusTypeDef status = APP_WiFi_SendSetPassphrasePmk(g_wifiJoinRequestedPassphrase, g_wifiJoinRequestedPassphraseLength);
      if (status == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_PMK;
      }
      else if (status == HAL_ERROR)
      {
        APP_WiFi_FailJoin("set passphrase failed", 0U, g_wifiJoinRequestedPassphraseLength);
      }
      break;
    }

    case APP_WIFI_JOIN_STEP_WAIT_PMK:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_WSEC_PMK))
      {
        if (g_wifiLastIoctlStatus == 0U)
        {
          g_wifiJoinStep = APP_WIFI_JOIN_STEP_SEND_INFRA;
        }
        else
        {
          APP_WiFi_FailJoin("set wsec pmk failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_SEND_INFRA:
      if (APP_WiFi_SendSetInfraIoctl(1UL) == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_INFRA;
      }
      break;

    case APP_WIFI_JOIN_STEP_WAIT_INFRA:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_INFRA))
      {
        if (g_wifiLastIoctlStatus == 0U)
        {
          g_wifiJoinStep = APP_WIFI_JOIN_STEP_SEND_AUTH;
        }
        else
        {
          APP_WiFi_FailJoin("set infra failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_SEND_AUTH:
      if (APP_WiFi_SendSetAuthIoctl(APP_WIFI_WL_AUTH_OPEN_SYSTEM) == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_AUTH;
      }
      break;

    case APP_WIFI_JOIN_STEP_WAIT_AUTH:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_AUTH))
      {
        if (g_wifiLastIoctlStatus == 0U)
        {
          g_wifiJoinStep = APP_WIFI_JOIN_STEP_SEND_WPA_AUTH;
        }
        else
        {
          APP_WiFi_FailJoin("set auth failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_SEND_WPA_AUTH:
    {
      const HAL_StatusTypeDef status = APP_WiFi_SendSetWpaAuthIoctl((g_wifiJoinSecurity == APP_WIFI_JOIN_SECURITY_OPEN) ? APP_WIFI_WPA_AUTH_DISABLED : APP_WIFI_WPA2_AUTH_PSK);
      if (status == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_WPA_AUTH;
      }
      else if (status == HAL_ERROR)
      {
        (void)APP_WiFi_Platform_AbortFunction2Read();
        APP_WiFi_FailJoin("send wpa_auth failed", APP_WiFi_Platform_GetLastSdioStatus(), APP_WiFi_Platform_GetLastSdioError());
      }
      break;
    }

    case APP_WIFI_JOIN_STEP_WAIT_WPA_AUTH:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_WPA_AUTH))
      {
        if (g_wifiLastIoctlStatus == 0U)
        {
          g_wifiJoinStep = APP_WIFI_JOIN_STEP_SEND_SSID;
        }
        else
        {
          APP_WiFi_FailJoin("set wpa_auth failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_SEND_SSID:
    {
      const HAL_StatusTypeDef status = APP_WiFi_SendJoinSsidIoctl();
      if (status == HAL_OK)
      {
        g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_SSID_ACK;
      }
      else if (status == HAL_ERROR)
      {
        (void)APP_WiFi_Platform_AbortFunction2Read();
        APP_WiFi_FailJoin("send ssid failed", APP_WiFi_Platform_GetLastSdioStatus(), APP_WiFi_Platform_GetLastSdioError());
      }
      break;
    }

    case APP_WIFI_JOIN_STEP_WAIT_SSID_ACK:
      if ((g_wifiIoctlProbeCompleted != 0U) && (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_SSID))
      {
        if (g_wifiLastIoctlStatus == 0U)
        {
          APP_WiFi_Logf("[wifi] join: firmware accepted join request for ssid=\"%s\"\n",
                        g_wifiJoinRequestedSsid);
          g_wifiJoinStep = APP_WIFI_JOIN_STEP_WAIT_EVENTS;
          APP_WiFi_Logf("[wifi] join: waiting for link confirmation ssid=\"%s\"\n",
                        g_wifiJoinRequestedSsid);
        }
        else
        {
          APP_WiFi_FailJoin("set ssid ioctl failed", g_wifiLastIoctlStatus, 0U);
        }
      }
      break;

    case APP_WIFI_JOIN_STEP_WAIT_EVENTS:
      if ((g_wifiJoinStartTick != 0U) &&
          ((now - g_wifiJoinStartTick) > APP_WIFI_JOIN_LINK_TIMEOUT_MS))
      {
        APP_WiFi_FailJoin("link confirmation timeout", g_wifiLastIoctlStatus, g_wifiLastJoinEventReason);
        break;
      }

      if ((g_wifiIoctlProbeCompleted != 0U) &&
          ((g_wifiJoinLastPollTick == 0U) ||
           ((now - g_wifiJoinLastPollTick) >= APP_WIFI_JOIN_LINK_POLL_INTERVAL_MS)))
      {
        g_wifiJoinLastPollTick = now;
        if (APP_WiFi_SendGetBssidIoctl() == HAL_OK)
        {
        }
        else
        {
          (void)APP_WiFi_Platform_AbortFunction2Read();
          APP_WiFi_Logf("[wifi] join: send WLC_GET_BSSID failed sta=0x%08lX err=0x%08lX\n",
                        (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                        (unsigned long)APP_WiFi_Platform_GetLastSdioError());
        }
      }

      APP_WiFi_EvaluateJoinCompletion();
      break;

    case APP_WIFI_JOIN_STEP_IDLE:
    default:
      break;
  }
}

static uint8_t APP_WiFi_TryProbeSdpcmRxInternal(uint8_t requireInterruptHint)
{
  const uint8_t scanActive = ((g_wifiState == APP_WIFI_STATE_MAILBOX_READY) &&
                              (g_wifiScanIoctlCompleted != 0U) &&
                              (g_wifiScanCompleted == 0U) &&
                              (g_wifiScanAborted == 0U)) ? 1U : 0U;
  const uint8_t pendingIoctl = ((g_wifiState == APP_WIFI_STATE_MAILBOX_READY) &&
                                (g_wifiIoctlProbeSent != 0U) &&
                                (g_wifiIoctlProbeCompleted == 0U)) ? 1U : 0U;
  uint32_t frameInterrupt = 0U;
  uint8_t header[APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE] = {0};
  uint8_t chunkBuffer[64] = {0};
  uint8_t hwtagReadSize = APP_WIFI_SDPCM_HW_TAG_SIZE;
  uint16_t frameLength = 0U;
  uint16_t frameCheck = 0U;
  uint16_t remaining = 0U;
  uint16_t transferRemaining = 0U;
  uint16_t captured = APP_WIFI_SDPCM_HW_TAG_SIZE;
  uint16_t rawFrameCaptured = 0U;

  if (requireInterruptHint != 0U)
  {
    if (APP_WiFi_Platform_GetFunction2RxInterruptStatus(&frameInterrupt) != HAL_OK)
    {
      return 0U;
    }

    if ((frameInterrupt & APP_WIFI_SDPCM_FRAME_AVAILABLE_MASK) == 0U)
    {
      return 0U;
    }
  }

  if (APP_WiFi_Platform_Fn2Read(header, APP_WIFI_SDPCM_HW_TAG_SIZE) != HAL_OK)
  {
    if ((requireInterruptHint == 0U) && (scanActive == 0U) && (pendingIoctl == 0U))
    {
      return 0U;
    }

    (void)APP_WiFi_Platform_AbortFunction2Read();

    if (APP_WiFi_Platform_Fn2Read(chunkBuffer, (uint16_t)sizeof(chunkBuffer)) != HAL_OK)
    {
      APP_WiFi_Logf("[wifi] sdpcm: hwtag read failed irq=0x%08lX sta=0x%08lX err=0x%08lX\n",
                    (unsigned long)frameInterrupt,
                    (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioError());
      (void)APP_WiFi_Platform_AbortFunction2Read();
      (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
      return 0U;
    }

    memcpy(header, chunkBuffer, APP_WIFI_SDPCM_HW_TAG_SIZE);
    hwtagReadSize = (uint8_t)sizeof(chunkBuffer);
    captured = (hwtagReadSize > APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE) ? APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE : hwtagReadSize;
    if (captured > APP_WIFI_SDPCM_HW_TAG_SIZE)
    {
      memcpy(&header[APP_WIFI_SDPCM_HW_TAG_SIZE],
             &chunkBuffer[APP_WIFI_SDPCM_HW_TAG_SIZE],
             (size_t)(captured - APP_WIFI_SDPCM_HW_TAG_SIZE));
    }

    if (APP_WIFI_SDPCM_TRACE_ENABLE != 0U)
    {
      APP_WiFi_Logf("[wifi] sdpcm: hwtag fallback used irq=0x%08lX\n",
                    (unsigned long)frameInterrupt);
    }
  }

  frameLength = APP_WiFi_ReadLe16(&header[0]);
  frameCheck = APP_WiFi_ReadLe16(&header[2]);

  if ((requireInterruptHint == 0U) && (frameLength == 0U) && (frameCheck == 0U))
  {
    return 0U;
  }

  if ((frameLength == 0U) || ((uint16_t)(frameLength ^ frameCheck) != 0xFFFFU))
  {
    APP_WiFi_Logf("[wifi] sdpcm: hwtag mismatch irq=0x%08lX len=0x%04X chk=0x%04X\n",
                  (unsigned long)frameInterrupt,
                  (unsigned int)frameLength,
                  (unsigned int)frameCheck);
    APP_WiFi_LogSdpcmBytes("[wifi] sdpcm: raw=",
                           header,
                           captured);
    (void)APP_WiFi_Platform_AbortFunction2Read();
    (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
    return 0U;
  }

  if (frameLength > APP_WIFI_SDPCM_MAX_FRAME_LEN)
  {
    APP_WiFi_Logf("[wifi] sdpcm: frame too large irq=0x%08lX len=%u\n",
                  (unsigned long)frameInterrupt,
                  (unsigned int)frameLength);
    (void)APP_WiFi_Platform_AbortFunction2Read();
    (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
    return 0U;
  }

  memset(g_wifiRxFrameBuffer, 0, frameLength);

  if (hwtagReadSize >= APP_WIFI_SDPCM_FN2_BLOCK_SIZE)
  {
    const uint16_t roundedFrameLength = (uint16_t)(((uint32_t)frameLength + (APP_WIFI_SDPCM_FN2_BLOCK_SIZE - 1U)) &
                                                   ~(uint32_t)(APP_WIFI_SDPCM_FN2_BLOCK_SIZE - 1U));
    transferRemaining = (roundedFrameLength > hwtagReadSize) ? (uint16_t)(roundedFrameLength - hwtagReadSize) : 0U;
    remaining = (frameLength > hwtagReadSize) ? (uint16_t)(frameLength - hwtagReadSize) : 0U;
  }
  else
  {
    remaining = (frameLength > hwtagReadSize) ? (uint16_t)(frameLength - hwtagReadSize) : 0U;
    transferRemaining = remaining;
  }

  rawFrameCaptured = (frameLength < hwtagReadSize) ? frameLength : hwtagReadSize;
  memcpy(g_wifiRxFrameBuffer, header, rawFrameCaptured);

  while (transferRemaining > 0U)
  {
    const uint16_t chunkSize = (hwtagReadSize >= APP_WIFI_SDPCM_FN2_BLOCK_SIZE) ?
                               APP_WIFI_SDPCM_FN2_BLOCK_SIZE :
                               APP_WiFi_GetSdpcmChunkSize(transferRemaining);
    uint16_t copyLength = 0U;
    HAL_StatusTypeDef status = HAL_ERROR;

    if (chunkSize == 1U)
    {
      status = APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN2, 0U, &chunkBuffer[0]);
    }
    else
    {
      status = APP_WiFi_Platform_Fn2Read(chunkBuffer, chunkSize);
    }

    if (status != HAL_OK)
    {
      APP_WiFi_Logf("[wifi] sdpcm: payload read failed irq=0x%08lX len=%u rem=%u sta=0x%08lX err=0x%08lX\n",
                    (unsigned long)frameInterrupt,
                    (unsigned int)frameLength,
                    (unsigned int)transferRemaining,
                    (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioError());
      (void)APP_WiFi_Platform_AbortFunction2Read();
      (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
      return 0U;
    }

    if (captured < APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE)
    {
      copyLength = (uint16_t)(APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE - captured);
      if (copyLength > remaining)
      {
        copyLength = remaining;
      }
      if (copyLength > chunkSize)
      {
        copyLength = chunkSize;
      }

      memcpy(&header[captured], chunkBuffer, copyLength);
      captured = (uint16_t)(captured + copyLength);
    }

    if (rawFrameCaptured < frameLength)
    {
      uint16_t rawCopyLength = chunkSize;

      if (rawCopyLength > (uint16_t)(frameLength - rawFrameCaptured))
      {
        rawCopyLength = (uint16_t)(frameLength - rawFrameCaptured);
      }

      if (rawCopyLength > 0U)
      {
        memcpy(&g_wifiRxFrameBuffer[rawFrameCaptured], chunkBuffer, rawCopyLength);
        rawFrameCaptured = (uint16_t)(rawFrameCaptured + rawCopyLength);
      }
    }

    transferRemaining = (uint16_t)(transferRemaining - chunkSize);
    if (remaining > chunkSize)
    {
      remaining = (uint16_t)(remaining - chunkSize);
    }
    else
    {
      remaining = 0U;
    }
  }

  if (frameLength < APP_WIFI_SDPCM_HEADER_SIZE)
  {
    APP_WiFi_Logf("[wifi] sdpcm: short frame irq=0x%08lX len=%u\n",
                  (unsigned long)frameInterrupt,
                  (unsigned int)frameLength);
    (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
    return 1U;
  }

  APP_WiFi_UpdateSdpcmCredit(header);
  APP_WiFi_RecordSdpcmHeader(header, frameLength, frameInterrupt);
  if ((g_wifiLastSdpcmChannel == APP_WIFI_SDPCM_CHANNEL_CONTROL) &&
      (captured >= (APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE)))
  {
    const uint32_t command = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + 0U]);
    const uint32_t payloadLength = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + 4U]);
    const uint32_t flags = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + 8U]);
    const uint32_t status = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + 12U]);

    g_wifiLastIoctlCommand = command;
    g_wifiLastIoctlFlags = flags;
    g_wifiLastIoctlStatus = status;

    if ((command == APP_WIFI_WLC_GET_VERSION) &&
        (payloadLength >= sizeof(uint32_t)) &&
        (captured >= (APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + sizeof(uint32_t))))
    {
      g_wifiLastIoctlValue = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE]);
    }

    if ((command == APP_WIFI_WLC_GET_VAR) &&
        (payloadLength >= APP_WIFI_MAC_ADDRESS_SIZE) &&
        (captured >= (APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + APP_WIFI_MAC_ADDRESS_SIZE)))
    {
      memcpy(g_wifiLastMacAddress,
             &header[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
             APP_WIFI_MAC_ADDRESS_SIZE);
      APP_WiFi_LogMacAddress("[wifi] iovar: rsp cur_etheraddr mac=", g_wifiLastMacAddress);
    }

    if ((command == APP_WIFI_WLC_GET_BSSID) &&
        (payloadLength >= APP_WIFI_MAC_ADDRESS_SIZE) &&
        (captured >= (APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + APP_WIFI_MAC_ADDRESS_SIZE)))
    {
      memcpy(g_wifiLastBssid,
             &header[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
             APP_WIFI_MAC_ADDRESS_SIZE);
      APP_WiFi_LogMacAddress("[wifi] join: rsp bssid=", g_wifiLastBssid);
    }

    APP_WiFi_Logf("[wifi] ioctl: rsp cmd=%lu id=%u len=%lu status=0x%08lX value=0x%08lX\n",
                  (unsigned long)command,
                  (unsigned int)((flags & APP_WIFI_SDPCM_IOCTL_ID_MASK) >> APP_WIFI_SDPCM_IOCTL_ID_SHIFT),
                  (unsigned long)payloadLength,
                  (unsigned long)status,
                  (unsigned long)g_wifiLastIoctlValue);

    if (((flags & APP_WIFI_SDPCM_IOCTL_ID_MASK) >> APP_WIFI_SDPCM_IOCTL_ID_SHIFT) == g_wifiIoctlRequestId)
    {
      const APP_WiFi_IovarRequest_t pendingIovarRequest = g_wifiPendingIovarRequest;

      g_wifiIoctlProbeCompleted = 1U;

      if ((command == APP_WIFI_WLC_GET_VERSION) && (status == 0U))
      {
        g_wifiVersionProbeCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_UP) && (status == 0U))
      {
        g_wifiUpProbeCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_SET_VAR) &&
          (pendingIovarRequest == APP_WIFI_IOVAR_REQUEST_TX_GLOM_SET))
      {
        g_wifiTxGlomCompleted = 1U;
        if (status != 0U)
        {
          APP_WiFi_Logf("[wifi] iovar: %s returned 0x%08lX, continuing\n",
                        APP_WIFI_IOVAR_TX_GLOM,
                        (unsigned long)status);
        }
      }

      if ((command == APP_WIFI_WLC_SET_VAR) &&
          (pendingIovarRequest == APP_WIFI_IOVAR_REQUEST_APSTA_SET))
      {
        g_wifiApstaCompleted = 1U;
        if (status != 0U)
        {
          APP_WiFi_Logf("[wifi] iovar: %s returned 0x%08lX, continuing\n",
                        APP_WIFI_IOVAR_APSTA,
                        (unsigned long)status);
        }
      }

      if ((command == APP_WIFI_WLC_SET_VAR) &&
          (pendingIovarRequest == APP_WIFI_IOVAR_REQUEST_COUNTRY_SET) &&
          (status == 0U))
      {
        g_wifiCountryCompleted = 1U;
      }
      else if ((command == APP_WIFI_WLC_SET_VAR) &&
               (pendingIovarRequest == APP_WIFI_IOVAR_REQUEST_COUNTRY_SET))
      {
        APP_WiFi_Logf("[wifi] iovar: %s failed status=0x%08lX\n",
                      APP_WIFI_IOVAR_COUNTRY,
                      (unsigned long)status);
      }

      if ((command == APP_WIFI_WLC_SET_GMODE) && (status == 0U))
      {
        g_wifiGmodeCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_GET_VAR) &&
          (pendingIovarRequest == APP_WIFI_IOVAR_REQUEST_CUR_ETHERADDR_GET) &&
          (status == 0U))
      {
        g_wifiMacProbeCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_SET_VAR) &&
          (pendingIovarRequest == APP_WIFI_IOVAR_REQUEST_EVENT_MSGS_SET) &&
          (status == 0U))
      {
        g_wifiEventMaskCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_SET_SCANSUPPRESS) && (status == 0U))
      {
        g_wifiScanSuppressCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_SET_PM) && (status == 0U))
      {
        g_wifiPmProbeCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_GET_BSSID) &&
          (g_wifiLinkState == APP_WIFI_LINK_STATE_CONNECTING) &&
          (g_wifiJoinStep == APP_WIFI_JOIN_STEP_WAIT_EVENTS))
      {
        uint8_t bssidValid = 0U;

        for (uint8_t index = 0U; index < APP_WIFI_MAC_ADDRESS_SIZE; index++)
        {
          if ((g_wifiLastBssid[index] != 0x00U) && (g_wifiLastBssid[index] != 0xFFU))
          {
            bssidValid = 1U;
            break;
          }
        }

        if ((status == 0U) && (bssidValid != 0U))
        {
          g_wifiJoinAuthenticated = 1U;
          g_wifiJoinLinkReady = 1U;
          g_wifiJoinSecurityComplete = 1U;
          APP_WiFi_LogMacAddress("[wifi] join: link confirmed bssid=", g_wifiLastBssid);
          APP_WiFi_EvaluateJoinCompletion();
        }
        else
        {
          APP_WiFi_Logf("[wifi] join: link not ready status=0x%08lX bssidValid=%u\n",
                        (unsigned long)status,
                        (unsigned int)bssidValid);
        }
      }

      if ((command == APP_WIFI_WLC_SET_VAR) &&
          (pendingIovarRequest == APP_WIFI_IOVAR_REQUEST_ESCAN_SET) &&
          (status == 0U) &&
          (g_wifiScanIoctlCompleted == 0U))
      {
        g_wifiScanIoctlCompleted = 1U;
        g_wifiLastScanPollTick = 0U;
        APP_WiFi_Logf("[wifi] scan: escan ioctl accepted id=%u\n",
                      (unsigned int)g_wifiIoctlRequestId);
      }

      if ((command == APP_WIFI_WLC_GET_VAR) || (command == APP_WIFI_WLC_SET_VAR))
      {
        g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_NONE;
      }
    }
  }
  else if (g_wifiLastSdpcmChannel == APP_WIFI_SDPCM_CHANNEL_EVENT)
  {
    APP_WiFi_HandleAsyncEvent(header, captured);
  }
  else if (g_wifiLastSdpcmChannel == APP_WIFI_SDPCM_CHANNEL_DATA)
  {
    if (g_wifiLastSdpcmHeaderLength >= (APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_DATA_PADDING_SIZE))
    {
      const APP_WiFi_BdcHeader_t *bdcHeader = (const APP_WiFi_BdcHeader_t *)&g_wifiRxFrameBuffer[g_wifiLastSdpcmHeaderLength];
      const uint16_t payloadOffset = (uint16_t)(g_wifiLastSdpcmHeaderLength +
                                                APP_WIFI_SDPCM_BDC_HEADER_SIZE +
                                                ((uint16_t)bdcHeader->data_offset << 2U));

      if (payloadOffset < frameLength)
      {
        APP_WiFi_LwIP_ProcessEthernetFrame(&g_wifiRxFrameBuffer[payloadOffset],
                                           (uint16_t)(frameLength - payloadOffset));
      }
    }
  }
  if ((frameLength == APP_WIFI_SDPCM_HEADER_SIZE) &&
      (g_wifiLastSdpcmHeaderLength == APP_WIFI_SDPCM_HEADER_SIZE))
  {
    if (APP_WIFI_SDPCM_TRACE_ENABLE != 0U)
    {
      APP_WiFi_Logf("[wifi] sdpcm: credit-only seq=%u ch=%s credit=%u diff=%u flow=%u\n",
                    (unsigned int)g_wifiLastSdpcmSequence,
                    APP_WiFi_SdpcmChannelToString(g_wifiLastSdpcmChannel),
                    (unsigned int)g_wifiBusCredit,
                    (unsigned int)g_wifiBusCreditDiff,
                    (unsigned int)g_wifiLastSdpcmFlowControl);
    }
  }
  (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
  return 1U;
}

static uint32_t APP_WiFi_DrainSdpcmRxQueue(uint8_t maxFrames)
{
  uint32_t framesDrained = 0U;

  if (maxFrames == 0U)
  {
    return 0U;
  }

  if (APP_WiFi_TryProbeSdpcmRxInternal(1U) != 0U)
  {
    framesDrained = 1U;
  }
  else if (APP_WiFi_TryProbeSdpcmRxInternal(0U) != 0U)
  {
    framesDrained = 1U;
  }
  else
  {
    return 0U;
  }

  while (framesDrained < maxFrames)
  {
    if (APP_WiFi_TryProbeSdpcmRxInternal(0U) == 0U)
    {
      break;
    }

    ++framesDrained;
  }

  return framesDrained;
}

static void APP_WiFi_PollPendingIoctlResponse(void)
{
  uint32_t framesDrained = 0U;

  if ((g_wifiState != APP_WIFI_STATE_MAILBOX_READY) ||
      (g_wifiIoctlProbeSent == 0U) ||
      (g_wifiIoctlProbeCompleted != 0U))
  {
    return;
  }

  framesDrained = APP_WiFi_DrainSdpcmRxQueue(4U);

  while ((framesDrained == 0U) && (framesDrained < 4U) &&
         (APP_WiFi_TryProbeSdpcmRxInternal(0U) != 0U))
  {
    framesDrained++;
  }

  while ((framesDrained < 4U) && (APP_WiFi_TryProbeSdpcmRxInternal(0U) != 0U))
  {
    framesDrained++;
  }
}

static void APP_WiFi_PollActiveScanResults(void)
{
  uint32_t framesDrained = 0U;
  const uint32_t now = HAL_GetTick();

  if ((g_wifiState != APP_WIFI_STATE_MAILBOX_READY) ||
      (g_wifiScanIoctlCompleted == 0U) ||
      (g_wifiScanCompleted != 0U) ||
      (g_wifiScanAborted != 0U))
  {
    return;
  }

  if ((now - g_wifiLastScanPollTick) < APP_WIFI_SCAN_ACTIVE_POLL_MS)
  {
    return;
  }

  g_wifiLastScanPollTick = now;
  framesDrained = APP_WiFi_DrainSdpcmRxQueue(8U);

  while ((framesDrained == 0U) && (framesDrained < 8U) &&
         (APP_WiFi_TryProbeSdpcmRxInternal(0U) != 0U))
  {
    framesDrained++;
  }

  while ((framesDrained < 8U) && (APP_WiFi_TryProbeSdpcmRxInternal(0U) != 0U))
  {
    framesDrained++;
  }

  if ((now - g_wifiScanStartTick) > APP_WIFI_SCAN_TIMEOUT_MS)
  {
    APP_WiFi_FailScan("timeout waiting for escan events");
  }
}

static void APP_WiFi_StartScanRequest(void)
{
  g_wifiScanRequested = 0U;
  g_wifiScanStep = APP_WIFI_SCAN_STEP_SEND_EVENT_MASK;
  g_wifiScanStepStartTick = HAL_GetTick();
  g_wifiScanStartTick = 0U;
  g_wifiScanSent = 0U;
  g_wifiScanIoctlCompleted = 0U;
  g_wifiScanCompleted = 0U;
  g_wifiScanAborted = 0U;
  g_wifiScanPartialCount = 0U;
  g_wifiScanResultCount = 0U;
  g_wifiLastScanSyncId = 0U;
  g_wifiLastScanBssCount = 0U;
  g_wifiLastScanRssi = 0;
  g_wifiLastScanChannel = 0U;
  memset(g_wifiLastScanSsid, 0, sizeof(g_wifiLastScanSsid));
  APP_WiFi_ClearCachedScanResults();
  APP_WiFi_Logf("[wifi] scan: starting requested escan sequence\n");
}

static void APP_WiFi_FailScan(const char *reason)
{
  if (g_wifiScanStep == APP_WIFI_SCAN_STEP_IDLE)
  {
    return;
  }

  (void)APP_WiFi_Platform_AbortFunction2Read();
  g_wifiScanStep = APP_WIFI_SCAN_STEP_IDLE;
  g_wifiScanRequested = 0U;
  g_wifiScanAborted = (g_wifiCachedScanResultCount == 0U) ? 1U : 0U;
  g_wifiScanCompleted = (g_wifiCachedScanResultCount != 0U) ? 1U : 0U;
  g_wifiScanIoctlCompleted = 0U;
  g_wifiIoctlProbeCompleted = 1U;
  g_wifiPendingIovarRequest = APP_WIFI_IOVAR_REQUEST_NONE;
  g_wifiLastIoctlStatus = APP_WIFI_LOCAL_STATUS_TIMEOUT;
  APP_WiFi_Logf("[wifi] scan: %s reason=\"%s\" cached=%lu\n",
                (g_wifiScanCompleted != 0U) ? "completed with partial cache" : "aborted",
                (reason != NULL) ? reason : "unknown",
                (unsigned long)g_wifiCachedScanResultCount);
}

static void APP_WiFi_ProcessScanRequest(void)
{
  const uint32_t now = HAL_GetTick();

  if (g_wifiState != APP_WIFI_STATE_MAILBOX_READY)
  {
    return;
  }

  if ((g_wifiScanRequested != 0U) &&
      (g_wifiScanStep == APP_WIFI_SCAN_STEP_IDLE) &&
      (g_wifiLinkState != APP_WIFI_LINK_STATE_CONNECTING) &&
      (g_wifiUpProbeCompleted != 0U) &&
      ((g_wifiIoctlProbeSent == 0U) || (g_wifiIoctlProbeCompleted != 0U)))
  {
    APP_WiFi_StartScanRequest();
  }

  if (g_wifiScanStep == APP_WIFI_SCAN_STEP_IDLE)
  {
    return;
  }

  if ((g_wifiIoctlProbeSent != 0U) &&
      (g_wifiIoctlProbeCompleted == 0U))
  {
    if ((now - g_wifiScanStepStartTick) > APP_WIFI_SCAN_STEP_TIMEOUT_MS)
    {
      APP_WiFi_FailScan("timeout waiting for setup ioctl response");
    }
    return;
  }

  switch (g_wifiScanStep)
  {
    case APP_WIFI_SCAN_STEP_SEND_EVENT_MASK:
    {
      const HAL_StatusTypeDef status = APP_WiFi_SendSetEventMsgsIovar();
      if (status == HAL_OK)
      {
        g_wifiEventMaskSent = 1U;
        g_wifiScanStep = APP_WIFI_SCAN_STEP_WAIT_EVENT_MASK;
        g_wifiScanStepStartTick = now;
      }
      else if (status == HAL_ERROR)
      {
        APP_WiFi_Logf("[wifi] scan: event_msgs setup send failed, continuing with defaults\n");
        (void)APP_WiFi_Platform_AbortFunction2Read();
        g_wifiScanStep = APP_WIFI_SCAN_STEP_SEND_SCAN_SUPPRESS;
        g_wifiScanStepStartTick = now;
      }
      break;
    }

    case APP_WIFI_SCAN_STEP_WAIT_EVENT_MASK:
      APP_WiFi_Logf("[wifi] scan: event_msgs setup status=0x%08lX\n",
                    (unsigned long)g_wifiLastIoctlStatus);
      g_wifiScanStep = APP_WIFI_SCAN_STEP_SEND_SCAN_SUPPRESS;
      g_wifiScanStepStartTick = now;
      break;

    case APP_WIFI_SCAN_STEP_SEND_SCAN_SUPPRESS:
    {
      const HAL_StatusTypeDef status = APP_WiFi_SendClearScanSuppressIoctl();
      if (status == HAL_OK)
      {
        g_wifiScanSuppressSent = 1U;
        g_wifiScanStep = APP_WIFI_SCAN_STEP_WAIT_SCAN_SUPPRESS;
        g_wifiScanStepStartTick = now;
      }
      else if (status == HAL_ERROR)
      {
        APP_WiFi_Logf("[wifi] scan: scansuppress setup send failed, continuing\n");
        (void)APP_WiFi_Platform_AbortFunction2Read();
        g_wifiScanStep = APP_WIFI_SCAN_STEP_SEND_PM_OFF;
        g_wifiScanStepStartTick = now;
      }
      break;
    }

    case APP_WIFI_SCAN_STEP_WAIT_SCAN_SUPPRESS:
      APP_WiFi_Logf("[wifi] scan: scansuppress setup status=0x%08lX\n",
                    (unsigned long)g_wifiLastIoctlStatus);
      g_wifiScanStep = APP_WIFI_SCAN_STEP_SEND_PM_OFF;
      g_wifiScanStepStartTick = now;
      break;

    case APP_WIFI_SCAN_STEP_SEND_PM_OFF:
    {
      const HAL_StatusTypeDef status = APP_WiFi_SendSetPmOffIoctl();
      if (status == HAL_OK)
      {
        g_wifiPmProbeSent = 1U;
        g_wifiScanStep = APP_WIFI_SCAN_STEP_WAIT_PM_OFF;
        g_wifiScanStepStartTick = now;
      }
      else if (status == HAL_ERROR)
      {
        APP_WiFi_Logf("[wifi] scan: PM_OFF setup send failed, continuing\n");
        (void)APP_WiFi_Platform_AbortFunction2Read();
        g_wifiScanStep = APP_WIFI_SCAN_STEP_SEND_ESCAN;
        g_wifiScanStepStartTick = now;
      }
      break;
    }

    case APP_WIFI_SCAN_STEP_WAIT_PM_OFF:
      APP_WiFi_Logf("[wifi] scan: PM_OFF setup status=0x%08lX\n",
                    (unsigned long)g_wifiLastIoctlStatus);
      g_wifiScanStep = APP_WIFI_SCAN_STEP_SEND_ESCAN;
      g_wifiScanStepStartTick = now;
      break;

    case APP_WIFI_SCAN_STEP_SEND_ESCAN:
    {
      const HAL_StatusTypeDef status = APP_WiFi_SendEscanIovar();
      if (status == HAL_OK)
      {
        g_wifiScanSent = 1U;
        g_wifiScanStep = APP_WIFI_SCAN_STEP_WAIT_ESCAN_ACK;
        g_wifiScanStepStartTick = now;
        g_wifiScanStartTick = now;
      }
      else if (status == HAL_ERROR)
      {
        APP_WiFi_FailScan("send escan failed");
      }
      break;
    }

    case APP_WIFI_SCAN_STEP_WAIT_ESCAN_ACK:
      if (g_wifiScanIoctlCompleted != 0U)
      {
        g_wifiScanStep = APP_WIFI_SCAN_STEP_WAIT_RESULTS;
        g_wifiScanStepStartTick = now;
      }
      else if ((g_wifiIoctlProbeCompleted != 0U) &&
               (g_wifiLastIoctlCommand == APP_WIFI_WLC_SET_VAR))
      {
        APP_WiFi_FailScan("escan ioctl rejected");
      }
      break;

    case APP_WIFI_SCAN_STEP_WAIT_RESULTS:
      if (g_wifiScanCompleted != 0U)
      {
        APP_WiFi_Logf("[wifi] scan: cached results=%lu\n",
                      (unsigned long)g_wifiCachedScanResultCount);
        g_wifiScanStep = APP_WIFI_SCAN_STEP_IDLE;
      }
      else if (g_wifiScanAborted != 0U)
      {
        g_wifiScanStep = APP_WIFI_SCAN_STEP_IDLE;
      }
      break;

    case APP_WIFI_SCAN_STEP_IDLE:
    default:
      g_wifiScanStep = APP_WIFI_SCAN_STEP_IDLE;
      break;
  }
}

static void APP_WiFi_LogStateDetails(APP_WiFiState_t state)
{
  switch (state)
  {
    case APP_WIFI_STATE_SDIO_ENUMERATED:
      APP_WiFi_Logf("[wifi] sdio: OCR=0x%08lX RCA=0x%04X\n",
                    (unsigned long)APP_WiFi_Platform_GetSdioOcr(),
                    (unsigned int)APP_WiFi_Platform_GetSdioRca());
      break;

    case APP_WIFI_STATE_CCCR_READY:
      APP_WiFi_Logf("[wifi] cccr: rev=0x%02X sdrev=0x%02X caps=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetCccrRevision(),
                    (unsigned int)APP_WiFi_Platform_GetCccrSdRevision(),
                    (unsigned int)APP_WiFi_Platform_GetCccrCapabilities());
      break;

    case APP_WIFI_STATE_FUNCTION1_READY:
    case APP_WIFI_STATE_FUNCTION2_READY:
      APP_WiFi_Logf("[wifi] functions: IOEN=0x%02X IORDY=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetCccrIoEnable(),
                    (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
      break;

    case APP_WIFI_STATE_BUS_READY:
      APP_WiFi_Logf("[wifi] bus: BICTRL=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetCccrBusControl());
      break;

    case APP_WIFI_STATE_CMD53_READY:
      APP_WiFi_Logf("[wifi] cmd53: word=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetLastCmd53Word());
      break;

    case APP_WIFI_STATE_CLOCK_READY:
    case APP_WIFI_STATE_HT_CLOCK_READY:
      APP_WiFi_Logf("[wifi] clock: CSR=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetChipClockCsr());
      break;

    case APP_WIFI_STATE_BACKPLANE_READY:
      APP_WiFi_Logf("[wifi] backplane: window=0x%08lX word=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetBackplaneWindowBase(),
                    (unsigned long)APP_WiFi_Platform_GetLastBackplaneWord());
      break;

    case APP_WIFI_STATE_INTERRUPTS_READY:
      APP_WiFi_Logf("[wifi] irq: INTEN=0x%02X SEP=0x%02X oob=%lu\n",
                    (unsigned int)APP_WiFi_Platform_GetCccrInterruptEnable(),
                    (unsigned int)APP_WiFi_Platform_GetSepInterruptControl(),
                    (unsigned long)APP_WiFi_GetOobInterruptCount());
      break;

    case APP_WIFI_STATE_RESOURCES_READY:
      APP_WiFi_Logf("[wifi] resources: fw=%luB entry=0x%08lX nvram=%luB nvramAddr=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetFirmwareSize(),
                    (unsigned long)APP_WiFi_Platform_GetFirmwareEntryWord(),
                    (unsigned long)APP_WiFi_Platform_GetNvramSize(),
                    (unsigned long)APP_WiFi_Platform_GetNvramStagingAddress());
      break;

    case APP_WIFI_STATE_FIRMWARE_STAGED:
      APP_WiFi_Logf("[wifi] staged: firmware=%luB\n",
                    (unsigned long)APP_WiFi_Platform_GetFirmwareBytesStaged());
      break;

    case APP_WIFI_STATE_NVRAM_STAGED:
      APP_WiFi_Logf("[wifi] staged: nvram=%luB trailer=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetNvramBytesStaged(),
                    (unsigned long)APP_WiFi_Platform_GetNvramTrailerWord());
      break;

    case APP_WIFI_STATE_ARM_RELEASED:
    case APP_WIFI_STATE_FIRMWARE_BOOTED:
      APP_WiFi_Logf("[wifi] wlancore: IOCTRL=0x%02X RESETCTRL=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetWlanCoreIoCtrl(),
                    (unsigned int)APP_WiFi_Platform_GetWlanCoreResetCtrl());
      break;

    case APP_WIFI_STATE_SHARED_READY:
      APP_WiFi_Logf("[wifi] shared: addr=0x%08lX flags=0x%08lX console=0x%08lX fwid=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetWlanSharedAddress(),
                    (unsigned long)APP_WiFi_Platform_GetWlanSharedFlags(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleAddress(),
                    (unsigned long)APP_WiFi_Platform_GetFirmwareId());
      break;

    case APP_WIFI_STATE_CONSOLE_READY:
      APP_WiFi_Logf("[wifi] console: buf=0x%08lX size=%lu wr=%lu out=%lu\n",
                    (unsigned long)APP_WiFi_Platform_GetConsoleBufferAddress(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleBufferSize(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleWriteIndex(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleOutIndex());
      break;

    case APP_WIFI_STATE_MAILBOX_READY:
      APP_WiFi_Logf("[wifi] mailbox: INT=0x%08lX data=0x%08lX SDIOirq=%lu OOB=%lu\n",
                    (unsigned long)APP_WiFi_Platform_GetInterruptStatus(),
                    (unsigned long)APP_WiFi_Platform_GetHostMailboxData(),
                    (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                    (unsigned long)APP_WiFi_GetOobInterruptCount());
      break;

    case APP_WIFI_STATE_READY:
      APP_WiFi_Logf("[wifi] bus-up: clock=0x%02X IORDY=0x%02X hostMask=0x%08lX fnMask=0x%08lX\n",
                    (unsigned int)APP_WiFi_Platform_GetChipClockCsr(),
                    (unsigned int)APP_WiFi_Platform_GetCccrIoReady(),
                    (unsigned long)APP_WiFi_Platform_GetHostInterruptMask(),
                    (unsigned long)APP_WiFi_Platform_GetFunctionInterruptMask());
      break;

    case APP_WIFI_STATE_ERROR:
      APP_WiFi_Logf("[wifi] error: lastStatus=0x%08lX lastError=0x%08lX clock=0x%02X\n",
                    (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioError(),
                    (unsigned int)APP_WiFi_Platform_GetChipClockCsr());
      break;

    default:
      break;
  }
}

static void APP_WiFi_LogPeriodicHeartbeat(void)
{
  const uint32_t now = HAL_GetTick();

  if ((now - g_wifiLastHeartbeatTick) < APP_WIFI_HEARTBEAT_MS)
  {
    return;
  }

  g_wifiLastHeartbeatTick = now;

  if (g_wifiState == APP_WIFI_STATE_MAILBOX_READY)
  {
    if (g_wifiIoctlProbeSent == 0U)
    {
      (void)APP_WiFi_SendGetVersionIoctl();
    }
    else if ((g_wifiVersionProbeCompleted != 0U) &&
             (g_wifiUpProbeSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      if ((g_wifiTxGlomSent == 0U) ||
          (g_wifiApstaSent == 0U) ||
          (g_wifiCountrySent == 0U) ||
          (g_wifiGmodeSent == 0U))
      {
        g_wifiTxGlomSent = 1U;
        g_wifiTxGlomCompleted = 1U;
        g_wifiApstaSent = 1U;
        g_wifiApstaCompleted = 1U;
        g_wifiCountrySent = 1U;
        g_wifiCountryCompleted = 1U;
        g_wifiGmodeSent = 1U;
        g_wifiGmodeCompleted = 1U;
        APP_WiFi_Logf("[wifi] pre-join: skipping txglom/apsta/country/gmode and restoring known-good control chain\n");
      }

      if (APP_WiFi_SendUpIoctl() == HAL_OK)
      {
        g_wifiUpProbeSent = 1U;
      }
    }
    else if ((g_wifiUpProbeCompleted != 0U) &&
             (g_wifiMacProbeSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      const HAL_StatusTypeDef macProbeStatus = APP_WiFi_SendGetCurEtheraddrIovar();

      if (macProbeStatus == HAL_OK)
      {
        g_wifiMacProbeSent = 1U;
      }
      else if (macProbeStatus == HAL_ERROR)
      {
        g_wifiMacProbeSent = 1U;
        g_wifiMacProbeCompleted = 1U;
        APP_WiFi_Logf("[wifi] pre-join: cur_etheraddr probe skipped after send failure, continuing bring-up\n");
      }
    }
    else if ((g_wifiMacProbeCompleted != 0U) &&
             (g_wifiEventMaskSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      const HAL_StatusTypeDef eventMaskStatus = APP_WiFi_SendSetEventMsgsIovar();

      if (eventMaskStatus == HAL_OK)
      {
        g_wifiEventMaskSent = 1U;
      }
      else if (eventMaskStatus == HAL_ERROR)
      {
        (void)APP_WiFi_Platform_AbortFunction2Read();
        g_wifiEventMaskSent = 1U;
        g_wifiEventMaskCompleted = 1U;
        APP_WiFi_Logf("[wifi] iovar: event_msgs bypassed after send failure, continuing bring-up\n");
      }
    }
    else if ((g_wifiEventMaskCompleted != 0U) &&
             (g_wifiScanSuppressSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      const HAL_StatusTypeDef scanSuppressStatus = APP_WiFi_SendClearScanSuppressIoctl();

      if (scanSuppressStatus == HAL_OK)
      {
        g_wifiScanSuppressSent = 1U;
      }
      else if (scanSuppressStatus == HAL_ERROR)
      {
        (void)APP_WiFi_Platform_AbortFunction2Read();
        g_wifiScanSuppressSent = 1U;
        g_wifiScanSuppressCompleted = 1U;
        APP_WiFi_Logf("[wifi] ioctl: WLC_SET_SCANSUPPRESS bypassed after send failure, continuing bring-up\n");
      }
    }
    else if ((g_wifiScanSuppressCompleted != 0U) &&
             (g_wifiPmProbeSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      const HAL_StatusTypeDef pmStatus = APP_WiFi_SendSetPmOffIoctl();

      if (pmStatus == HAL_OK)
      {
        g_wifiPmProbeSent = 1U;
      }
      else if (pmStatus == HAL_ERROR)
      {
        (void)APP_WiFi_Platform_AbortFunction2Read();
        g_wifiPmProbeSent = 1U;
        g_wifiPmProbeCompleted = 1U;
        APP_WiFi_Logf("[wifi] ioctl: WLC_SET_PM bypassed after send failure, continuing bring-up\n");
      }
    }
    else if ((g_wifiScanRequested != 0U) &&
             (g_wifiPmProbeCompleted != 0U) &&
             (g_wifiScanSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      APP_WiFi_ProcessScanRequest();
    }

    (void)APP_WiFi_Platform_ProbeConsole();
    (void)APP_WiFi_Platform_ProbeMailbox();

    if (APP_WIFI_HEARTBEAT_LOG_ENABLE != 0U)
    {
      APP_WiFi_Logf("[wifi] heartbeat: state=%s sdioIrq=%lu oob=%lu mailbox=0x%08lX int=0x%08lX consoleWr=%lu consoleOut=%lu sdpcm=%lu len=%u ch=%u seq=%u credit=%u flow=%u ioctlSent=%u ioctlDone=%u ioctlCmd=%lu ioctlStatus=0x%08lX verOk=%u glomSent=%u glomOk=%u apstaSent=%u apstaOk=%u countrySent=%u countryOk=%u upSent=%u upOk=%u gmSent=%u gmOk=%u macSent=%u macOk=%u evtSent=%u evtOk=%u supSent=%u supOk=%u pmSent=%u pmOk=%u scanSent=%u scanIoctl=%u scanDone=%u scanAbort=%u scanPart=%lu scanRes=%lu\n",
                    APP_WiFi_StateToString(g_wifiState),
                    (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                    (unsigned long)APP_WiFi_GetOobInterruptCount(),
                    (unsigned long)APP_WiFi_Platform_GetHostMailboxData(),
                    (unsigned long)APP_WiFi_Platform_GetInterruptStatus(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleWriteIndex(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleOutIndex(),
                    (unsigned long)g_wifiSdpcmFrameCount,
                    (unsigned int)g_wifiLastSdpcmFrameLength,
                    (unsigned int)g_wifiLastSdpcmChannel,
                    (unsigned int)g_wifiLastSdpcmSequence,
                    (unsigned int)g_wifiBusCredit,
                    (unsigned int)g_wifiLastSdpcmFlowControl,
                    (unsigned int)g_wifiIoctlProbeSent,
                    (unsigned int)g_wifiIoctlProbeCompleted,
                    (unsigned long)g_wifiLastIoctlCommand,
                    (unsigned long)g_wifiLastIoctlStatus,
                    (unsigned int)g_wifiVersionProbeCompleted,
                    (unsigned int)g_wifiTxGlomSent,
                    (unsigned int)g_wifiTxGlomCompleted,
                    (unsigned int)g_wifiApstaSent,
                    (unsigned int)g_wifiApstaCompleted,
                    (unsigned int)g_wifiCountrySent,
                    (unsigned int)g_wifiCountryCompleted,
                    (unsigned int)g_wifiUpProbeSent,
                    (unsigned int)g_wifiUpProbeCompleted,
                    (unsigned int)g_wifiGmodeSent,
                    (unsigned int)g_wifiGmodeCompleted,
                    (unsigned int)g_wifiMacProbeSent,
                    (unsigned int)g_wifiMacProbeCompleted,
                    (unsigned int)g_wifiEventMaskSent,
                    (unsigned int)g_wifiEventMaskCompleted,
                    (unsigned int)g_wifiScanSuppressSent,
                    (unsigned int)g_wifiScanSuppressCompleted,
                    (unsigned int)g_wifiPmProbeSent,
                    (unsigned int)g_wifiPmProbeCompleted,
                    (unsigned int)g_wifiScanSent,
                    (unsigned int)g_wifiScanIoctlCompleted,
                    (unsigned int)g_wifiScanCompleted,
                    (unsigned int)g_wifiScanAborted,
                    (unsigned long)g_wifiScanPartialCount,
                    (unsigned long)g_wifiScanResultCount);
    }
    return;
  }

  if (g_wifiState == APP_WIFI_STATE_ERROR)
  {
    if (APP_WIFI_HEARTBEAT_LOG_ENABLE != 0U)
    {
      APP_WiFi_Logf("[wifi] heartbeat: state=%s lastStatus=0x%08lX lastError=0x%08lX sdioIrq=%lu oob=%lu\n",
                    APP_WiFi_StateToString(g_wifiState),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioError(),
                    (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                    (unsigned long)APP_WiFi_GetOobInterruptCount());
    }
    return;
  }

  if (APP_WIFI_HEARTBEAT_LOG_ENABLE != 0U)
  {
    APP_WiFi_Logf("[wifi] heartbeat: state=%s sdioIrq=%lu oob=%lu\n",
                  APP_WiFi_StateToString(g_wifiState),
                  (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                  (unsigned long)APP_WiFi_GetOobInterruptCount());
  }
}

void APP_WiFi_Task(void *argument)
{
  (void)argument;

  /*
   * This task is the future home of the AP6181 bring-up path:
   * SDMMC/WWD init, join, DHCP, and socket transport should stay here
   * instead of running inside the GUI task.
   */
  for (;;)
  {
    switch (g_wifiState)
    {
      case APP_WIFI_STATE_IDLE:
        APP_WiFi_Platform_SetResetPin(GPIO_PIN_SET);
        APP_WiFi_SetState(APP_WIFI_STATE_WAIT_STACK);
        osDelay(APP_WIFI_STACK_WAIT_MS);
        break;

      case APP_WIFI_STATE_WAIT_STACK:
        /*
         * FreeRTOS is already running here. When we start integrating the
         * AP6181 stack, any one-time scheduler-dependent initialization can
         * move into this state.
         */
        APP_WiFi_SetState(APP_WIFI_STATE_RESET_ASSERT);
        break;

      case APP_WIFI_STATE_RESET_ASSERT:
        /*
         * AP6181 needs a clean hardware reset before we attempt any SDIO/WWD
         * bring-up. We do that here instead of inside board init so the whole
         * sequence lives in the dedicated Wi-Fi task.
         */
        APP_WiFi_ResetNoCreditStall();
        APP_WiFi_Platform_SetResetPin(GPIO_PIN_RESET);
        APP_WiFi_SetState(APP_WIFI_STATE_RESET_RELEASE);
        osDelay(APP_WIFI_RESET_ASSERT_MS);
        break;

      case APP_WIFI_STATE_RESET_RELEASE:
        APP_WiFi_Platform_SetResetPin(GPIO_PIN_SET);
        APP_WiFi_SetState(APP_WIFI_STATE_MODULE_SETTLE);
        osDelay(APP_WIFI_RESET_RELEASE_GUARD_MS);
        break;

      case APP_WIFI_STATE_MODULE_SETTLE:
        /*
         * Keep a short settle window after reset deassertion. We intentionally
         * stop here for now: AP6181 is not a standard SD card, so we do not
         * call MX_SDMMC1_SD_Init()/HAL_SD_Init() from this task. The future
         * WWD/SDIO bring-up will replace the BRINGUP_PENDING placeholder.
         */
        APP_WiFi_SetState(APP_WIFI_STATE_BRINGUP_PENDING);
        osDelay(APP_WIFI_MODULE_SETTLE_MS);
        break;

      case APP_WIFI_STATE_BRINGUP_PENDING:
        if (APP_WiFi_Platform_SdioHostInit() == HAL_OK)
        {
          APP_WiFi_SetState(APP_WIFI_STATE_SDIO_HOST_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] pre-download setup failed: INTEN=0x%02X SEP=0x%02X IORDY=0x%02X\n",
                        (unsigned int)APP_WiFi_Platform_GetCccrInterruptEnable(),
                        (unsigned int)APP_WiFi_Platform_GetSepInterruptControl(),
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_SDIO_HOST_READY:
        if (APP_WiFi_Platform_SdioEnumerate() == HAL_OK)
        {
          APP_WiFi_SetState(APP_WIFI_STATE_SDIO_ENUMERATED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] pre-download setup failed: INTEN=0x%02X SEP=0x%02X IORDY=0x%02X\n",
                        (unsigned int)APP_WiFi_Platform_GetCccrInterruptEnable(),
                        (unsigned int)APP_WiFi_Platform_GetSepInterruptControl(),
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_SDIO_ENUMERATED:
        if (APP_WiFi_Platform_ProbeCccr() == HAL_OK)
        {
          /*
           * This is the first real SDIO register-access milestone:
           * the module now responds to CMD52 reads on Fn0/CCCR space.
           * We keep the state separate so the next step can build on this
           * point when enabling functions and moving toward CMD53 transfers.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_CCCR_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_CCCR_READY:
        if (APP_WiFi_Platform_EnableFunction1() == HAL_OK)
        {
          /*
           * At this point the SDIO backplane function is not only enabled in
           * IOEN, but also reports ready in IORDY. The next steps can safely
           * build on top of a live Function 1 endpoint.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_FUNCTION1_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FUNCTION1_READY:
        if (APP_WiFi_Platform_ConfigureBus() == HAL_OK)
        {
          /*
           * Card-side CCCR block size and bus width are now configured, and the
           * local SDMMC host has been switched to match. This is the last
           * lightweight setup step before we start touching CMD53 transfers.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_BUS_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_BUS_READY:
        if (APP_WiFi_Platform_RunCmd53SmokeTest() == HAL_OK)
        {
          /*
           * First minimal CMD53 smoke test succeeded. We now have proof that
           * the SDIO data path is alive, not just the command path.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_CMD53_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_CMD53_READY:
        if (APP_WiFi_Platform_RequestAlpClock() == HAL_OK)
        {
          /*
           * The module now acknowledges the ALP clock request through the
           * Function 1 clock CSR path. That gives us a solid base for the
           * upcoming backplane-window and higher-level WWD bring-up steps.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_CLOCK_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_CLOCK_READY:
        if (APP_WiFi_Platform_RunBackplaneSmokeTest() == HAL_OK)
        {
          /*
           * We can now set a backplane window and pull a real 32-bit word from
           * chipcommon space through Fn1/CMD53. This is the first point where
           * we know the AP6181 backplane path, not just raw SDIO transport, is
           * alive.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_BACKPLANE_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_BACKPLANE_READY:
        if (APP_WiFi_Platform_RunBackplaneWriteSmokeTest() == HAL_OK)
        {
          /*
           * The backplane write path now works too. From here we pivot to the
           * vendor-aligned pre-download sequence: enable Function 2 first, then
           * program the CCCR/OOB interrupt path, then start firmware staging.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_FUNCTION2_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_HT_CLOCK_READY:
        if ((APP_WiFi_Platform_ConfigurePostFirmwareBus() == HAL_OK) &&
            (APP_WiFi_Platform_WaitForFunction2Ready() == HAL_OK))
        {
          /*
           * This is the closest checkpoint to the official 43362 SDIO bring-up:
           * firmware has booted, HT is available, the post-download backplane
           * interrupt masks/watermark are programmed, and Function 2 finally
           * reports ready. From here on, shared/console/mailbox probing is
           * treated as follow-on diagnostics rather than the bring-up gate.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] post-fw bus setup failed: IORDY=0x%02X clock=0x%02X hostMask=0x%08lX fnMask=0x%08lX\n",
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady(),
                        (unsigned int)APP_WiFi_Platform_GetChipClockCsr(),
                        (unsigned long)APP_WiFi_Platform_GetHostInterruptMask(),
                        (unsigned long)APP_WiFi_Platform_GetFunctionInterruptMask());
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FUNCTION2_READY:
        if (APP_WiFi_Platform_EnableFunction2() == HAL_OK)
        {
          /*
           * Function 2 is enabled in IOEN before firmware download, matching
           * the vendor SDIO bring-up sequence.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_INTERRUPTS_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_INTERRUPTS_READY:
        if ((APP_WiFi_Platform_ConfigureInterruptPath() == HAL_OK) &&
            (APP_WiFi_Platform_PrepareFirmwareDownload() == HAL_OK) &&
            (APP_WiFi_Platform_ProbeFirmwareResources() == HAL_OK))
        {
          /*
           * This keeps the vendor-style pre-download setup together: pull-ups
           * off, separate OOB control programmed, CCCR INTEN set for master +
           * Function 2, the download-side core prep is complete, and the
           * firmware/NVRAM resource layout is computed.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_RESOURCES_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] pre-download setup failed: INTEN=0x%02X SEP=0x%02X IORDY=0x%02X\n",
                        (unsigned int)APP_WiFi_Platform_GetCccrInterruptEnable(),
                        (unsigned int)APP_WiFi_Platform_GetSepInterruptControl(),
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_RESOURCES_READY:
        if (APP_WiFi_Platform_StageFirmwareImage() == HAL_OK)
        {
          /*
           * The firmware blob has now been copied into WLAN RAM. We still
           * stage NVRAM separately so download failures remain easy to isolate.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_FIRMWARE_STAGED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FIRMWARE_STAGED:
        if (APP_WiFi_Platform_StageNvramImage() == HAL_OK)
        {
          /*
           * NVRAM and its trailer are now in place at the top of WLAN RAM.
           * The next stage can release the WLAN ARM core.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_NVRAM_STAGED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_NVRAM_STAGED:
        if (APP_WiFi_Platform_ReleaseWlanArmCore() == HAL_OK)
        {
          /*
           * The WLAN ARM wrapper has now been taken out of reset using the
           * same minimal sequence as the vendor flow.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_ARM_RELEASED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_ARM_RELEASED:
        if (APP_WiFi_Platform_WaitForFirmwareBoot() == HAL_OK)
        {
          /*
           * This is still the lightweight wrapper-based boot check: the WLAN
           * ARM core reports clock enabled and not in reset.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_FIRMWARE_BOOTED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FIRMWARE_BOOTED:
        if (APP_WiFi_Platform_RequestHtClock() == HAL_OK)
        {
          /*
           * This matches the next vendor checkpoint after firmware download:
           * request HT and wait for HT_AVAIL before treating the SDIO/WLAN bus
           * as really up.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_HT_CLOCK_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] fw boot HT wait failed: clock=0x%02X IORDY=0x%02X\n",
                        (unsigned int)APP_WiFi_Platform_GetChipClockCsr(),
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_READY:
        if (APP_WiFi_Platform_ProbeSharedMemory() == HAL_OK)
        {
          APP_WiFi_SetState(APP_WIFI_STATE_SHARED_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_LogPeriodicHeartbeat();
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_SHARED_READY:
        if (APP_WiFi_Platform_ProbeConsole() == HAL_OK)
        {
          /*
           * The firmware's console structure is now readable through the
           * shared-area pointer, so we have a concrete log-buffer location and
           * cursor state available for later debug-log extraction.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_CONSOLE_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] console probe pending: console=0x%08lX buf=0x%08lX size=%lu wr=%lu out=%lu\n",
                        (unsigned long)APP_WiFi_Platform_GetConsoleAddress(),
                        (unsigned long)APP_WiFi_Platform_GetConsoleBufferAddress(),
                        (unsigned long)APP_WiFi_Platform_GetConsoleBufferSize(),
                        (unsigned long)APP_WiFi_Platform_GetConsoleWriteIndex(),
                        (unsigned long)APP_WiFi_Platform_GetConsoleOutIndex());
          APP_WiFi_LogPeriodicHeartbeat();
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_CONSOLE_READY:
        if (APP_WiFi_Platform_ProbeMailbox() == HAL_OK)
        {
          /*
          * We can now read the host-facing mailbox and interrupt-status path
           * after firmware boot, on top of the already-parsed shared/console
           * state. That gives the next bring-up steps a stable checkpoint
           * before we start leaning harder on higher-level bus-up assumptions
           * from the vendor stack.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_MAILBOX_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_LogPeriodicHeartbeat();
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_MAILBOX_READY:
      {
        uint32_t drainedFrames = 0U;
        uint8_t extraRounds = 0U;

        APP_WiFi_ProcessJoinRetry();
        APP_WiFi_ProcessJoinRequest();
        APP_WiFi_PollPendingIoctlResponse();
        APP_WiFi_ProcessConnectedLinkRecovery();
        APP_WiFi_ProcessScanRequest();
        APP_WiFi_PollActiveScanResults();
        drainedFrames = APP_WiFi_DrainSdpcmRxQueue(APP_WIFI_RX_DRAIN_BURST_MAIN);
        APP_WiFi_LwIP_Service();
        /*
         * TCP handshakes are timing-sensitive on ESP01S AT firmware. LwIP may
         * enqueue replies while we are draining RX frames; run a second pass only
         * when there was RX activity or pending TX to reduce idle-loop overhead.
         */
        if ((drainedFrames != 0U) || (APP_WiFi_LwIP_HasPendingTx() != 0U))
        {
          (void)APP_WiFi_DrainSdpcmRxQueue(APP_WIFI_RX_DRAIN_BURST_EXTRA);
          APP_WiFi_LwIP_Service();

          while (extraRounds < APP_WIFI_RX_DRAIN_EXTRA_ROUNDS)
          {
            uint32_t roundDrained = APP_WiFi_DrainSdpcmRxQueue(APP_WIFI_RX_DRAIN_BURST_EXTRA);
            if ((roundDrained == 0U) && (APP_WiFi_LwIP_HasPendingTx() == 0U))
            {
              break;
            }

            APP_WiFi_LwIP_Service();
            extraRounds++;
          }
        }
        APP_WiFi_LogPeriodicHeartbeat();
        osDelay(APP_WIFI_STACK_WAIT_MS);
        break;
      }

      case APP_WIFI_STATE_ERROR:
        APP_WiFi_LogPeriodicHeartbeat();
      default:
        osDelay(APP_WIFI_POLL_MS);
        break;
    }
  }
}
