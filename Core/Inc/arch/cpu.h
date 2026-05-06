#ifndef __ARCH_CPU_H__
#define __ARCH_CPU_H__

#include <stdint.h>

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN    4321
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER    LITTLE_ENDIAN
#endif

#endif /* __ARCH_CPU_H__ */
