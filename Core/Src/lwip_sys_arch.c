#include "arch/sys_arch.h"

#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"

void sys_init(void)
{
}

void sys_deinit(void)
{
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
  const UBaseType_t queueSize = (size > 0) ? (UBaseType_t)size : 6U;

  if (mbox == NULL)
  {
    return ERR_VAL;
  }

  *mbox = xQueueCreate(queueSize, sizeof(void *));
  if (*mbox == NULL)
  {
    return ERR_MEM;
  }

  return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
  if ((mbox == NULL) || (*mbox == NULL))
  {
    return;
  }

  vQueueDelete(*mbox);
  *mbox = NULL;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
  void *message = msg;

  if ((mbox == NULL) || (*mbox == NULL))
  {
    return;
  }

  (void)xQueueSend(*mbox, &message, portMAX_DELAY);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
  void *message = msg;

  if ((mbox == NULL) || (*mbox == NULL))
  {
    return ERR_VAL;
  }

  return (xQueueSend(*mbox, &message, 0U) == pdTRUE) ? ERR_OK : ERR_MEM;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
  void *message = NULL;
  void **target = (msg != NULL) ? msg : &message;
  TickType_t startTick;
  TickType_t elapsedTicks;
  BaseType_t received;

  if ((mbox == NULL) || (*mbox == NULL))
  {
    if (msg != NULL)
    {
      *msg = NULL;
    }
    return SYS_ARCH_TIMEOUT;
  }

  startTick = xTaskGetTickCount();

  if (timeout == 0U)
  {
    received = xQueueReceive(*mbox, target, portMAX_DELAY);
  }
  else
  {
    received = xQueueReceive(*mbox, target, pdMS_TO_TICKS(timeout));
  }

  if (received != pdTRUE)
  {
    if (msg != NULL)
    {
      *msg = NULL;
    }
    return SYS_ARCH_TIMEOUT;
  }

  elapsedTicks = xTaskGetTickCount() - startTick;
  if (elapsedTicks == 0U)
  {
    elapsedTicks = 1U;
  }

  return (u32_t)(elapsedTicks * (TickType_t)portTICK_PERIOD_MS);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
  void *message = NULL;
  void **target = (msg != NULL) ? msg : &message;

  if ((mbox == NULL) || (*mbox == NULL))
  {
    if (msg != NULL)
    {
      *msg = NULL;
    }
    return SYS_MBOX_EMPTY;
  }

  if (xQueueReceive(*mbox, target, 0U) == pdTRUE)
  {
    return 0U;
  }

  if (msg != NULL)
  {
    *msg = NULL;
  }
  return SYS_MBOX_EMPTY;
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
  if (sem == NULL)
  {
    return ERR_VAL;
  }

  *sem = xSemaphoreCreateBinary();
  if (*sem == NULL)
  {
    return ERR_MEM;
  }

  if (count != 0U)
  {
    (void)xSemaphoreGive(*sem);
  }

  return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
  if ((sem == NULL) || (*sem == NULL))
  {
    return;
  }

  vSemaphoreDelete(*sem);
  *sem = NULL;
}

void sys_sem_signal(sys_sem_t *sem)
{
  if ((sem == NULL) || (*sem == NULL))
  {
    return;
  }

  (void)xSemaphoreGive(*sem);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
  TickType_t startTick;
  TickType_t elapsedTicks;
  BaseType_t taken;

  if ((sem == NULL) || (*sem == NULL))
  {
    return SYS_ARCH_TIMEOUT;
  }

  startTick = xTaskGetTickCount();

  if (timeout == 0U)
  {
    taken = xSemaphoreTake(*sem, portMAX_DELAY);
  }
  else
  {
    taken = xSemaphoreTake(*sem, pdMS_TO_TICKS(timeout));
  }

  if (taken != pdTRUE)
  {
    return SYS_ARCH_TIMEOUT;
  }

  elapsedTicks = xTaskGetTickCount() - startTick;
  if (elapsedTicks == 0U)
  {
    elapsedTicks = 1U;
  }

  return (u32_t)(elapsedTicks * (TickType_t)portTICK_PERIOD_MS);
}

err_t sys_mutex_new(sys_mutex_t *mutex)
{
  if (mutex == NULL)
  {
    return ERR_VAL;
  }

  *mutex = xSemaphoreCreateMutex();
  if (*mutex == NULL)
  {
    return ERR_MEM;
  }

  return ERR_OK;
}

void sys_mutex_free(sys_mutex_t *mutex)
{
  if ((mutex == NULL) || (*mutex == NULL))
  {
    return;
  }

  vSemaphoreDelete(*mutex);
  *mutex = NULL;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
  if ((mutex == NULL) || (*mutex == NULL))
  {
    return;
  }

  (void)xSemaphoreTake(*mutex, portMAX_DELAY);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
  if ((mutex == NULL) || (*mutex == NULL))
  {
    return;
  }

  (void)xSemaphoreGive(*mutex);
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
  return (mbox != NULL) && (*mbox != NULL);
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
  if (mbox != NULL)
  {
    *mbox = NULL;
  }
}

int sys_sem_valid(sys_sem_t *sem)
{
  return (sem != NULL) && (*sem != NULL);
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
  if (sem != NULL)
  {
    *sem = NULL;
  }
}

int sys_mutex_valid(sys_mutex_t *mutex)
{
  return (mutex != NULL) && (*mutex != NULL);
}

void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
  if (mutex != NULL)
  {
    *mutex = NULL;
  }
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
  BaseType_t result;
  sys_thread_t threadHandle = NULL;
  const uint16_t stackWords = (stacksize > 0) ? (uint16_t)(stacksize / (int)sizeof(StackType_t)) : 1U;

  result = xTaskCreate(thread,
                       name,
                       (configSTACK_DEPTH_TYPE)stackWords,
                       arg,
                       (UBaseType_t)prio,
                       &threadHandle);

  if (result != pdPASS)
  {
    return NULL;
  }

  LWIP_ASSERT("Error creating thread", threadHandle != NULL);
  return threadHandle;
}

void sys_thread_exit(void)
{
  vTaskDelete(NULL);
}

void sys_thread_free(sys_thread_t *task)
{
  if ((task != NULL) && (*task != NULL))
  {
    vTaskDelete(*task);
    *task = NULL;
  }
}

u32_t sys_now(void)
{
  return (u32_t)(xTaskGetTickCount() * (TickType_t)portTICK_PERIOD_MS);
}

u32_t sys_jiffies(void)
{
  return (u32_t)xTaskGetTickCount();
}

sys_prot_t sys_arch_protect(void)
{
  taskENTER_CRITICAL();
  return 1U;
}

void sys_arch_unprotect(sys_prot_t pval)
{
  (void)pval;
  taskEXIT_CRITICAL();
}
