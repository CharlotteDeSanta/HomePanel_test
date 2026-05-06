#ifndef __ARCH_SYS_ARCH_H__
#define __ARCH_SYS_ARCH_H__

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

typedef SemaphoreHandle_t sys_sem_t;
typedef QueueHandle_t sys_mbox_t;
typedef TaskHandle_t sys_thread_t;
typedef SemaphoreHandle_t sys_mutex_t;
typedef uint32_t sys_prot_t;

#define SYS_ARCH_DECL_PROTECT(lev)          sys_prot_t lev
#define SYS_ARCH_PROTECT(lev)               lev = sys_arch_protect()
#define SYS_ARCH_UNPROTECT(lev)             sys_arch_unprotect(lev)

sys_prot_t sys_arch_protect(void);
void sys_arch_unprotect(sys_prot_t pval);

#endif /* __ARCH_SYS_ARCH_H__ */
