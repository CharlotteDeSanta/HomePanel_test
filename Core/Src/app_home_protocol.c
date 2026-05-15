#include "app_home_protocol.h"

#include <stdio.h>
#include <string.h>

static uint16_t APP_HomeProtocol_ReadLe16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static void APP_HomeProtocol_WriteLe16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value & 0xFFU);
  data[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void APP_HomeProtocol_ResetParser(APP_HomeProtocolParser_t *parser,
                                         APP_HomeProtocolError_t error)
{
  if (parser == NULL)
  {
    return;
  }

  parser->length = 0U;
  parser->lastError = error;
}

void APP_HomeProtocol_InitParser(APP_HomeProtocolParser_t *parser)
{
  if (parser == NULL)
  {
    return;
  }

  memset(parser->buffer, 0, sizeof(parser->buffer));
  parser->length = 0U;
  parser->lastError = APP_HOME_ERR_NONE;
}

APP_HomeProtocolParseResult_t APP_HomeProtocol_PushByte(APP_HomeProtocolParser_t *parser,
                                                        uint8_t byte,
                                                        APP_HomeProtocolFrame_t *frame)
{
  uint16_t payloadLength = 0U;
  uint16_t totalLength = 0U;
  uint16_t expectedCrc = 0U;
  uint16_t actualCrc = 0U;

  if ((parser == NULL) || (frame == NULL))
  {
    return APP_HOME_PARSE_ERROR;
  }

  if (parser->length == 0U)
  {
    if (byte == APP_HOME_PROTOCOL_SOF0)
    {
      parser->buffer[0] = byte;
      parser->length = 1U;
    }
    return APP_HOME_PARSE_NONE;
  }

  if (parser->length == 1U)
  {
    if (byte == APP_HOME_PROTOCOL_SOF1)
    {
      parser->buffer[1] = byte;
      parser->length = 2U;
    }
    else if (byte == APP_HOME_PROTOCOL_SOF0)
    {
      parser->buffer[0] = byte;
      parser->length = 1U;
    }
    else
    {
      APP_HomeProtocol_ResetParser(parser, APP_HOME_ERR_BAD_FRAME);
      return APP_HOME_PARSE_ERROR;
    }
    return APP_HOME_PARSE_NONE;
  }

  if (parser->length >= APP_HOME_PROTOCOL_MAX_FRAME_LEN)
  {
    APP_HomeProtocol_ResetParser(parser, APP_HOME_ERR_BAD_LENGTH);
    return APP_HOME_PARSE_ERROR;
  }

  parser->buffer[parser->length] = byte;
  parser->length++;

  if ((parser->length >= 3U) &&
      (parser->buffer[2] != APP_HOME_PROTOCOL_VERSION))
  {
    APP_HomeProtocol_ResetParser(parser, APP_HOME_ERR_BAD_FRAME);
    return APP_HOME_PARSE_ERROR;
  }

  if (parser->length < APP_HOME_PROTOCOL_HEADER_LEN)
  {
    return APP_HOME_PARSE_NONE;
  }

  payloadLength = APP_HomeProtocol_ReadLe16(&parser->buffer[7]);
  if (payloadLength > APP_HOME_PROTOCOL_MAX_PAYLOAD_LEN)
  {
    APP_HomeProtocol_ResetParser(parser, APP_HOME_ERR_BAD_LENGTH);
    return APP_HOME_PARSE_ERROR;
  }

  totalLength = (uint16_t)(APP_HOME_PROTOCOL_HEADER_LEN +
                           payloadLength +
                           APP_HOME_PROTOCOL_CRC_LEN);
  if (parser->length < totalLength)
  {
    return APP_HOME_PARSE_NONE;
  }

  if (parser->length > totalLength)
  {
    APP_HomeProtocol_ResetParser(parser, APP_HOME_ERR_BAD_LENGTH);
    return APP_HOME_PARSE_ERROR;
  }

  expectedCrc = APP_HomeProtocol_ReadLe16(&parser->buffer[APP_HOME_PROTOCOL_HEADER_LEN + payloadLength]);
  actualCrc = APP_HomeProtocol_Crc16(&parser->buffer[2], (uint16_t)(7U + payloadLength));
  if (expectedCrc != actualCrc)
  {
    printf("[proto] bad crc expected=0x%04X actual=0x%04X payloadLen=%u\n",
           (unsigned int)expectedCrc,
           (unsigned int)actualCrc,
           (unsigned int)payloadLength);
    APP_HomeProtocol_ResetParser(parser, APP_HOME_ERR_BAD_CRC);
    return APP_HOME_PARSE_ERROR;
  }

  frame->node = parser->buffer[3];
  frame->command = parser->buffer[4];
  frame->sequence = APP_HomeProtocol_ReadLe16(&parser->buffer[5]);
  frame->length = payloadLength;
  if (payloadLength != 0U)
  {
    memcpy(frame->payload, &parser->buffer[APP_HOME_PROTOCOL_HEADER_LEN], payloadLength);
  }

  APP_HomeProtocol_ResetParser(parser, APP_HOME_ERR_NONE);
  return APP_HOME_PARSE_FRAME;
}

uint16_t APP_HomeProtocol_BuildFrame(uint8_t node,
                                     uint8_t command,
                                     uint16_t sequence,
                                     const uint8_t *payload,
                                     uint16_t payloadLength,
                                     uint8_t *output,
                                     uint16_t outputSize)
{
  uint16_t totalLength = 0U;
  uint16_t crc = 0U;

  if ((output == NULL) ||
      (payloadLength > APP_HOME_PROTOCOL_MAX_PAYLOAD_LEN) ||
      ((payload == NULL) && (payloadLength != 0U)))
  {
    return 0U;
  }

  totalLength = (uint16_t)(APP_HOME_PROTOCOL_HEADER_LEN +
                           payloadLength +
                           APP_HOME_PROTOCOL_CRC_LEN);
  if (outputSize < totalLength)
  {
    return 0U;
  }

  output[0] = APP_HOME_PROTOCOL_SOF0;
  output[1] = APP_HOME_PROTOCOL_SOF1;
  output[2] = APP_HOME_PROTOCOL_VERSION;
  output[3] = node;
  output[4] = command;
  APP_HomeProtocol_WriteLe16(&output[5], sequence);
  APP_HomeProtocol_WriteLe16(&output[7], payloadLength);

  if (payloadLength != 0U)
  {
    memcpy(&output[APP_HOME_PROTOCOL_HEADER_LEN], payload, payloadLength);
  }

  crc = APP_HomeProtocol_Crc16(&output[2], (uint16_t)(7U + payloadLength));
  APP_HomeProtocol_WriteLe16(&output[APP_HOME_PROTOCOL_HEADER_LEN + payloadLength], crc);
  return totalLength;
}

uint16_t APP_HomeProtocol_Crc16(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFFU;
  uint16_t index = 0U;

  if (data == NULL)
  {
    return 0U;
  }

  for (index = 0U; index < length; index++)
  {
    uint8_t bit = 0U;
    crc ^= data[index];
    for (bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc = (uint16_t)((crc >> 1) ^ 0xA001U);
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

const char *APP_HomeProtocol_CommandToString(uint8_t command)
{
  switch (command)
  {
    case APP_HOME_CMD_HELLO:
      return "HELLO";
    case APP_HOME_CMD_TELEMETRY:
      return "TELEMETRY";
    case APP_HOME_CMD_CONTROL:
      return "CONTROL";
    case APP_HOME_CMD_STATUS:
      return "STATUS";
    case APP_HOME_CMD_HEARTBEAT:
      return "HEARTBEAT";
    case APP_HOME_CMD_ACK:
      return "ACK";
    case APP_HOME_CMD_ERR:
      return "ERR";
    default:
      return "UNKNOWN";
  }
}
