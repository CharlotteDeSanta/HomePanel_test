/**
  ******************************************************************************
  * @file           : hvac.h
  * @brief          : This file contains the public API of the HVAC module.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#ifndef __HVAC_H__
#define __HVAC_H__

#include <stdint.h>

#ifndef HVAC_USE_ZIGBEE_HANDLER
#define HVAC_USE_ZIGBEE_HANDLER  1
#endif

#ifndef HVAC_USE_WIRED_HANDLER
#define HVAC_USE_WIRED_HANDLER   1
#endif

#ifndef HVAC_USE_SIMU_HANDLER
#define HVAC_USE_SIMU_HANDLER    1
#endif


#define HVAC_UPDATE_PERIOD_MS       (2000)

#define HVAC_MAX_ROOM_COUNT         (3)
#define HVAC_MAX_ROOM_LABEL_LENGTH  (15)

#define HVAC_TEMP_HIGH_DIFF          (5.5f)
#define HVAC_TEMP_MED_DIFF           (3.5f)
#define HVAC_TEMP_LOW_DIFF           (1.5f)

/* System notifications flags */
#define HVAC_SYSTEM_CONTROLLER_READY         0x01
#define HVAC_SYSTEM_TELEMETRY_UPDATED        0x02
#define HVAC_SYSTEM_SETTINGS_UPDATED         0x04
#define HVAC_SYSTEM_TIME_UPDATED             0x08
#define HVAC_SYSTEM_WEATHER_UPDATED          0x10

typedef enum
{
    HVAC_MODE_OFF = 0,
    HVAC_MODE_COOL,
    HVAC_MODE_HEAT,
    HVAC_MODE_AUTO,
    HVAC_MODE_UNKNOWN
} HVAC_Mode_t;

typedef enum
{
    HVAC_FAN_OFF = 0,
    HVAC_FAN_LOW,
    HVAC_FAN_MED,
    HVAC_FAN_HIGH,
    HVAC_FAN_AUTO,
    HVAC_FAN_UNKNOWN
} HVAC_FanMode_t;

#define HVAC_EVENT_NONE        (0x0000)
#define HVAC_EVENT_TELEMETRY   (0x0001)
#define HVAC_EVENT_SETTINGS    (0x0002)
#define HVAC_EVENT_METADATA    (0x0004)
#define HVAC_EVENT_TIME        (0x0008)
#define HVAC_EVENT_WEATHER     (0x0010)


typedef uint32_t HVAC_EventType_t;


typedef struct
{
    uint64_t epoch_time;
} HVAC_EpochTime_t;

typedef struct
{
    uint64_t  epoch_time;
    float     current_temp;
    uint16_t  current_code;
    uint16_t  current_weekday;
    uint16_t  daily_code[5];
    float     daily_temp_min[5];
    float     daily_temp_max[5];
} HVAC_Weather_t;


typedef struct
{
    uint32_t       timestamp;
    float          temperature;
    float          humidity;
    HVAC_Mode_t    mode;
    HVAC_FanMode_t fan;
} HVAC_Telemetry_t;

typedef struct
{
    uint32_t        timestamp;
    float           temperature;
    HVAC_Mode_t     mode;
    HVAC_FanMode_t  fan;
} HVAC_Settings_t;

typedef struct
{
    uint32_t  timestamp;
    char      label[HVAC_MAX_ROOM_LABEL_LENGTH + 1];
} HVAC_Metadata_t;

typedef struct
{
    HVAC_Metadata_t   metadata;
    HVAC_Settings_t   settings;
    HVAC_Telemetry_t  telemetry;
} HVAC_Room_t;



typedef struct
{
    uint32_t            timestamp;
    uint16_t            roomId;
    HVAC_EventType_t    type;
    HVAC_Room_t         room;
    HVAC_EpochTime_t    time;
    HVAC_Weather_t      weather;
} HVAC_Event_t;

/**
 * @brief Initialize the HVAC Controller
 * @return pdTRUE if successful, pdFALSE otherwise
 */
bool xHVAC_ControllerInit();

/**
 * @brief Wait until the the HVAC Controller is up and running
 * @param[in] xTicksToWait number of ticks to wait. Use portMAX_DELAY to wait indefinitely.
 * @return pdTRUE if the HVAC Controller is ready, pdFALSE if the timeout expires.
 */
bool xHVAC_WaitControllerReady(uint32_t xTicksToWait);

/**
 * @brief Send a event message to the HVAC Controller
 * @param[in] event        Pointer to the event to send. The event will be copied into the queue.
 * @param[in] xTicksToWait number of ticks to wait for a space to be available in the event queue. Use portMAX_DELAY to wait indefinitely.
 * @return pdTRUE if the message was queued successfuly, pdFALSE otherwise.
 */
bool xHVAC_SendToController(const HVAC_Event_t* event, uint32_t xTicksToWait);

/**
 * @brief Receive a event message from the HVAC Controller
 * @param[out] event        Pointer to a buffer into which the received event will be copied.
 * @param[in]  xTicksToWait number of ticks to wait for an event to become available. Use portMAX_DELAY to wait indefinitely.
 * @return pdTRUE if the message was received successfuly, pdFALSE otherwise.
 */
bool xHVAC_ReceiveFromController(HVAC_Event_t* event, uint32_t xTicksToWait);

/**
 * @brief Retreive Room data
 * @param[in]  id  The room index
 * @param[out] pxRoom a pointer to a HVAC_Room_t structure to receive the room data;
 * @return pdTRUE if room data was succesfully retreived. pdFALSE otherwise
 */
bool xHVAC_GetRoomData(uint16_t idx, HVAC_Room_t* pxRoom, uint32_t xTicksToWait);

/**
 * @brief Retreive Room data
 * @param[in]  id  The room index
 * @param[out] pxRoom a pointer to a HVAC_Room_t structure to receive the room data;
 * @return pdTRUE if room data was succesfully retreived. pdFALSE otherwise
 */
bool xHVAC_SetRoomData(uint16_t idx, const HVAC_Room_t* pxRoom, uint32_t xTicksToWait);

/**
 * @brief Convert a HVAC Mode enum to string
 * @param[in] mode         the enum value to convert
 * @return the string representation of the enum or NULL if the enum is not valid.
 */
const char*     xHVAC_Mode2Str(HVAC_Mode_t mode);

/**
 * @brief Convert a string to a HVAC Mode enum
 * @param[in] mode         the string value to convert
 * @return the enum value, or HVAC_UNKNOWN if the string does not represent a valid value.
 */
HVAC_Mode_t  xHVAC_Str2Mode(const char* mode);

/**
 * @brief Convert a fanMode enum to string
 * @param[in] mode         the enum value to convert
 * @return the string representation of the enum or NULL if the enum is not valid.
 */
const char*     xHVAC_FanMode2Str(HVAC_FanMode_t mode);

/**
 * @brief Convert a string to a FanMode enum
 * @param[in] mode         the string value to convert
 * @return the enum value, or HVAC_FAN_UNKNOWN if the string does not represent a valid value.
 */
HVAC_FanMode_t  xHVAC_Str2FanMode(const char* mode);


/**
 * @brief Retrieve the current Weather data
 * @param[out] pxWeather  a pointer to a weather structure to be filled in with the current data
 * @return pdTRUE if the weather data was successfuly copied into the provided buffer
 */
bool  xHVAC_GetWeather(HVAC_Weather_t* pxWeather);

#endif
