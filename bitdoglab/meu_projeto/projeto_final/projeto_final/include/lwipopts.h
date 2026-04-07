#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS 1

#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

#define LWIP_TCP 1
#define LWIP_DNS 1

#define MEM_LIBC_MALLOC 1
#define MEMP_MEM_MALLOC 1

#define LWIP_TIMEVAL_PRIVATE 0

#define MEM_ALIGNMENT 4
#define MEM_SIZE 4000

#define MEMP_NUM_TCP_SEG 16
#define MEMP_NUM_PBUF 16
#define MEMP_NUM_TCP_PCB 5

#define PBUF_POOL_SIZE 16

#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_DHCP 1

#define TCP_MSS 1460
#define TCP_WND (4 * TCP_MSS)
#define TCP_SND_BUF (4 * TCP_MSS)

#endif