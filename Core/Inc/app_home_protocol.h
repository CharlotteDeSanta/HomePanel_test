#ifndef APP_HOME_PROTOCOL_H
#define APP_HOME_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_HOME_PROTOCOL_SOF0                 0xAAU
#define APP_HOME_PROTOCOL_SOF1                 0x55U
#define APP_HOME_PROTOCOL_VERSION              0x01U
#define APP_HOME_PROTOCOL_MAX_PAYLOAD_LEN      128U
#define APP_HOME_PROTOCOL_HEADER_LEN           9U
#define APP_HOME_PROTOCOL_CRC_LEN              2U
#define APP_HOME_PROTOCOL_MAX_FRAME_LEN        (APP_HOME_PROTOCOL_HEADER_LEN + \
                                                APP_HOME_PROTOCOL_MAX_PAYLOAD_LEN + \
                                                APP_HOME_PROTOCOL_CRC_LEN)

typedef enum
{
  APP_HOME_NODE_NONE = 0,
  APP_HOME_NODE_KITCHEN = 1,
  APP_HOME_NODE_LIVING = 2,
  APP_HOME_NODE_BEDROOM = 3
} APP_HomeProtocolNode_t;

typedef enum
{
  APP_HOME_CMD_HELLO = 0x01,
  APP_HOME_CMD_TELEMETRY = 0x02,
  APP_HOME_CMD_CONTROL = 0x03,
  APP_HOME_CMD_STATUS = 0x04,
  APP_HOME_CMD_HEARTBEAT = 0x05,
  APP_HOME_CMD_ACK = 0x06,
  APP_HOME_CMD_ERR = 0x07
} APP_HomeProtocolCommand_t;

typedef enum
{
  APP_HOME_ERR_NONE = 0x00,
  APP_HOME_ERR_BAD_FRAME = 0x01,
  APP_HOME_ERR_BAD_LENGTH = 0x02,
  APP_HOME_ERR_BAD_COMMAND = 0x03,
  APP_HOME_ERR_BAD_CRC = 0x04,
  APP_HOME_ERR_BAD_NODE = 0x05
} APP_HomeProtocolError_t;

typedef enum
{
  APP_HOME_PARSE_NONE = 0,
  APP_HOME_PARSE_FRAME,
  APP_HOME_PARSE_ERROR
} APP_HomeProtocolParseResult_t;

typedef struct
{
  uint8_t node;
  uint8_t command;
  uint16_t sequence;
  uint16_t length;
  uint8_t payload[APP_HOME_PROTOCOL_MAX_PAYLOAD_LEN];
} APP_HomeProtocolFrame_t;

typedef struct
{
  uint8_t buffer[APP_HOME_PROTOCOL_MAX_FRAME_LEN];
  uint16_t length;
  APP_HomeProtocolError_t lastError;
} APP_HomeProtocolParser_t;

void APP_HomeProtocol_InitParser(APP_HomeProtocolParser_t *parser);
APP_HomeProtocolParseResult_t APP_HomeProtocol_PushByte(APP_HomeProtocolParser_t *parser,
                                                        uint8_t byte,
                                                        APP_HomeProtocolFrame_t *frame);
uint16_t APP_HomeProtocol_BuildFrame(uint8_t node,
                                     uint8_t command,
                                     uint16_t sequence,
                                     const uint8_t *payload,
                                     uint16_t payloadLength,
                                     uint8_t *output,
                                     uint16_t outputSize);
uint16_t APP_HomeProtocol_Crc16(const uint8_t *data, uint16_t length);
const char *APP_HomeProtocol_CommandToString(uint8_t command);

#ifdef __cplusplus
}
#endif

#endif /* APP_HOME_PROTOCOL_H */
