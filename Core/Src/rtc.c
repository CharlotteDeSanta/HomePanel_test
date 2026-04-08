/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   This file provides code for the configuration
  *          of the RTC instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "rtc.h"

/* USER CODE BEGIN 0 */
static const uint32_t APP_RTC_BKP_MAGIC = 0x52444331UL;

static uint8_t parseBuildTimeComponent(const char* digits)
{
  if ((digits == NULL) || (digits[0] < '0') || (digits[0] > '9') || (digits[1] < '0') || (digits[1] > '9'))
  {
    return 0U;
  }

  return (uint8_t)((digits[0] - '0') * 10 + (digits[1] - '0'));
}

static uint8_t parseBuildMonth(void)
{
  const char* month = __DATE__;

  switch (month[0])
  {
  case 'J':
    if (month[1] == 'a')
    {
      return 1U;
    }
    return (month[2] == 'n') ? 6U : 7U;
  case 'F':
    return 2U;
  case 'M':
    return (month[2] == 'r') ? 3U : 5U;
  case 'A':
    return (month[1] == 'p') ? 4U : 8U;
  case 'S':
    return 9U;
  case 'O':
    return 10U;
  case 'N':
    return 11U;
  case 'D':
    return 12U;
  default:
    return 1U;
  }
}

static uint8_t parseBuildDay(void)
{
  const char* date = __DATE__;
  const uint8_t tens = (date[4] >= '0' && date[4] <= '9') ? (uint8_t)(date[4] - '0') : 0U;
  const uint8_t units = (date[5] >= '0' && date[5] <= '9') ? (uint8_t)(date[5] - '0') : 1U;

  return (uint8_t)(tens * 10U + units);
}

static uint16_t parseBuildYear(void)
{
  const char* date = __DATE__;
  return (uint16_t)((date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0'));
}

static uint8_t calculateWeekday(uint16_t year, uint8_t month, uint8_t day)
{
  static const uint8_t monthOffsets[] = { 0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U };

  year -= (month < 3U) ? 1U : 0U;
  return (uint8_t)((year + year / 4U - year / 100U + year / 400U + monthOffsets[month - 1U] + day) % 7U);
}

static uint32_t modelWeekdayToRtc(uint8_t weekday)
{
  switch (weekday % 7U)
  {
  case 0U:
    return RTC_WEEKDAY_SUNDAY;
  case 1U:
    return RTC_WEEKDAY_MONDAY;
  case 2U:
    return RTC_WEEKDAY_TUESDAY;
  case 3U:
    return RTC_WEEKDAY_WEDNESDAY;
  case 4U:
    return RTC_WEEKDAY_THURSDAY;
  case 5U:
    return RTC_WEEKDAY_FRIDAY;
  default:
    return RTC_WEEKDAY_SATURDAY;
  }
}

static uint8_t rtcWeekdayToModel(uint32_t weekday)
{
  switch (weekday)
  {
  case RTC_WEEKDAY_MONDAY:
    return 1U;
  case RTC_WEEKDAY_TUESDAY:
    return 2U;
  case RTC_WEEKDAY_WEDNESDAY:
    return 3U;
  case RTC_WEEKDAY_THURSDAY:
    return 4U;
  case RTC_WEEKDAY_FRIDAY:
    return 5U;
  case RTC_WEEKDAY_SATURDAY:
    return 6U;
  case RTC_WEEKDAY_SUNDAY:
  default:
    return 0U;
  }
}

/* USER CODE END 0 */

RTC_HandleTypeDef hrtc;

/* RTC init function */
void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  APP_RTC_DateTime_t buildDateTime = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != APP_RTC_BKP_MAGIC)
  {
    buildDateTime.year = parseBuildYear();
    buildDateTime.month = parseBuildMonth();
    buildDateTime.day = parseBuildDay();
    buildDateTime.weekday = calculateWeekday(buildDateTime.year, buildDateTime.month, buildDateTime.day);
    buildDateTime.hour = parseBuildTimeComponent(__TIME__);
    buildDateTime.minute = parseBuildTimeComponent(__TIME__ + 3);
    buildDateTime.second = parseBuildTimeComponent(__TIME__ + 6);

    if (APP_RTC_SetDateTime(&buildDateTime) == 0U)
    {
      Error_Handler();
    }
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspInit 0 */

  /* USER CODE END RTC_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* RTC clock enable */
    __HAL_RCC_RTC_ENABLE();
  /* USER CODE BEGIN RTC_MspInit 1 */

  /* USER CODE END RTC_MspInit 1 */
  }
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspDeInit 0 */

  /* USER CODE END RTC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_RTC_DISABLE();
  /* USER CODE BEGIN RTC_MspDeInit 1 */

  /* USER CODE END RTC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
uint8_t APP_RTC_GetDateTime(APP_RTC_DateTime_t* dateTime)
{
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};

  if ((dateTime == NULL) || (hrtc.Instance != RTC))
  {
    return 0U;
  }

  if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0U;
  }

  dateTime->year = (uint16_t)(2000U + date.Year);
  dateTime->month = (uint8_t)date.Month;
  dateTime->day = (uint8_t)date.Date;
  dateTime->weekday = rtcWeekdayToModel(date.WeekDay);
  dateTime->hour = (uint8_t)time.Hours;
  dateTime->minute = (uint8_t)time.Minutes;
  dateTime->second = (uint8_t)time.Seconds;

  return 1U;
}

uint8_t APP_RTC_SetDateTime(const APP_RTC_DateTime_t* dateTime)
{
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};
  const uint8_t modelWeekday = calculateWeekday(dateTime->year, dateTime->month, dateTime->day);

  if ((dateTime == NULL) || (hrtc.Instance != RTC))
  {
    return 0U;
  }

  time.Hours = dateTime->hour;
  time.Minutes = dateTime->minute;
  time.Seconds = dateTime->second;
  time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  time.StoreOperation = RTC_STOREOPERATION_RESET;

  date.WeekDay = (uint8_t)modelWeekdayToRtc(modelWeekday);
  date.Month = dateTime->month;
  date.Date = dateTime->day;
  date.Year = (uint8_t)(dateTime->year % 100U);

  if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0U;
  }

  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, APP_RTC_BKP_MAGIC);
  return 1U;
}

/* USER CODE END 1 */

