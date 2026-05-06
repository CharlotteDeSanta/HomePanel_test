#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "FreeRTOS.h"
#include "task.h"

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN    4321
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER    LITTLE_ENDIAN
#endif

#define LWIP_PLATFORM_DIAG(x)               do { printf x; } while (0)
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#define PACK_STRUCT_STRUCT                  __attribute__((packed))
#define PACK_STRUCT_FIELD(x)                x
#define PACK_STRUCT_FLD_8(x)                x
#define PACK_STRUCT_FLD_S(x)                x
#define LWIP_RAND()                         ((uint32_t)xTaskGetTickCount())

#endif /* __ARCH_CC_H__ */
