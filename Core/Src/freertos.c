/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_touchgfx.h"
#include "app_wifi.h"
#if defined(APP_ENABLE_SYSTEMVIEW) && (APP_ENABLE_SYSTEMVIEW == 1)
#include "SEGGER_SYSVIEW.h"
#endif

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define GUI_TASK_STACK_BYTES (4096U * 4U)
#define WIFI_TASK_STACK_BYTES (2048U * 4U)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
#if defined(APP_ENABLE_SYSTEMVIEW) && (APP_ENABLE_SYSTEMVIEW == 1)
static uint8_t g_systemViewStarted = 0U;
#endif

/* USER CODE END Variables */
/* Definitions for guiTask */
osThreadId_t guiTaskHandle;
const osThreadAttr_t guiTask_attributes = {
  .name = "guiTask",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for wifiTask */
osThreadId_t wifiTaskHandle;
const osThreadAttr_t wifiTask_attributes = {
  .name = "wifiTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityNormal7,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartGUITask(void *argument);
void StartWiFiTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
#if defined(APP_ENABLE_SYSTEMVIEW) && (APP_ENABLE_SYSTEMVIEW == 1)
  /*
   * Initialize SystemView before creating application tasks so startup,
   * task creation and early scheduler events are visible in the trace.
   */
  SEGGER_SYSVIEW_Conf();
#endif
  APP_WiFi_Init();

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of guiTask */
  guiTaskHandle = osThreadNew(StartGUITask, NULL, &guiTask_attributes);

  /* creation of wifiTask */
  wifiTaskHandle = osThreadNew(StartWiFiTask, NULL, &wifiTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartGUITask */
/**
  * @brief  Function implementing the guiTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartGUITask */
void StartGUITask(void *argument)
{
  /* USER CODE BEGIN StartGUITask */
#if defined(APP_ENABLE_SYSTEMVIEW) && (APP_ENABLE_SYSTEMVIEW == 1)
  /*
   * Start SystemView after scheduler and task context are active.
   * Starting too early (before scheduler start) can stall some FreeRTOS ports.
   */
  if (g_systemViewStarted == 0U)
  {
    g_systemViewStarted = 1U;
    SEGGER_SYSVIEW_Start();
  }
#endif
  /*
   * TouchGFX owns the UI thread. Keep all direct view/presenter/model updates
   * inside this task, and let future communication tasks exchange data through
   * queues instead of touching the UI directly.
   */
  TouchGFX_Task(argument);
  Error_Handler();
  /* USER CODE END StartGUITask */
}

/* USER CODE BEGIN Header_StartWiFiTask */
/**
  * @brief  Function implementing the wifiTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartWiFiTask */
void StartWiFiTask(void *argument)
{
  /* USER CODE BEGIN StartWiFiTask */
  APP_WiFi_Task(argument);
  Error_Handler();
  /* USER CODE END StartWiFiTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

