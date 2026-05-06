#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#include <stdio.h>

#define NO_SYS                              0
#define LWIP_TCPIP_CORE_LOCKING             0
#define LWIP_TCPIP_CORE_LOCKING_INPUT       0
#define SYS_LIGHTWEIGHT_PROT                1
#define LWIP_NETCONN                        1
#define LWIP_SOCKET                         1
#define LWIP_NETIF_API                      1
#define LWIP_DHCP                           1
#define LWIP_DNS                            1
#define LWIP_IPV4                           1
#define LWIP_IPV6                           0
#define LWIP_TCP                            1
#define LWIP_UDP                            1
#define LWIP_RAW                            1
#define LWIP_ICMP                           1
#define LWIP_ARP                            1
#define LWIP_IGMP                           0

#define LWIP_NETIF_HOSTNAME                 1
#define LWIP_NETIF_STATUS_CALLBACK          1
#define LWIP_NETIF_LINK_CALLBACK            1
#define LWIP_NETIF_TX_SINGLE_PBUF           1
#define LWIP_SO_RCVTIMEO                    1
#define LWIP_SO_SNDTIMEO                    1
#define SO_REUSE                            1
#define LWIP_PROVIDE_ERRNO                  1
#define DHCP_DOES_ARP_CHECK                 0
#define ARP_QUEUEING                        1
#define LWIP_STATS                          0

#define MEM_ALIGNMENT                       4
#define MEM_LIBC_MALLOC                     0
#define MEM_SIZE                            (32 * 1024)

#define PBUF_POOL_SIZE                      24
#define PBUF_POOL_BUFSIZE                   1600

#define MEMP_NUM_PBUF                       24
#define MEMP_NUM_UDP_PCB                    6
#define MEMP_NUM_TCP_PCB                    8
#define MEMP_NUM_TCP_PCB_LISTEN             4
#define MEMP_NUM_TCP_SEG                    32
#define MEMP_NUM_REASSDATA                  4
#define MEMP_NUM_NETCONN                    8
#define MEMP_NUM_NETBUF                     8
#define MEMP_NUM_SYS_TIMEOUT                12
#define MEMP_NUM_TCPIP_MSG_API              16
#define MEMP_NUM_TCPIP_MSG_INPKT            16

#define TCPIP_MBOX_SIZE                     16
#define TCPIP_THREAD_STACKSIZE              4096
#define TCPIP_THREAD_PRIO                   4
#define DEFAULT_RAW_RECVMBOX_SIZE           8
#define DEFAULT_TCP_RECVMBOX_SIZE           8
#define DEFAULT_UDP_RECVMBOX_SIZE           8
#define DEFAULT_ACCEPTMBOX_SIZE             8

#define TCP_MSS                             1460
#define TCP_WND                             (4 * TCP_MSS)
#define TCP_SND_BUF                         (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                    (2 * (TCP_SND_BUF / TCP_MSS))
#define TCP_LISTEN_BACKLOG                  1

#define LWIP_PLATFORM_ASSERT(x)             do { printf("lwIP assert: %s\n", (x)); for(;;) { } } while (0)

#endif /* __LWIPOPTS_H__ */
