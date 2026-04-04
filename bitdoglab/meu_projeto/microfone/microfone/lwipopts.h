/**
 * lwipopts.h — Configuração do lwIP para BitDogLab v6.3
 * ============================================================
 * Este arquivo É OBRIGATÓRIO quando se usa pico_cyw43_arch_lwip_poll.
 * O lwIP não possui defaults internos; toda configuração vem daqui.
 *
 * Baseado no template oficial do pico-examples:
 * https://github.com/raspberrypi/pico-examples/blob/master/pico_w/wifi/lwipopts.h
 * ============================================================
 */

#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

/* ── Ambiente: sem SO, single-threaded ──────────────────── */
#define NO_SYS 1               /* sem RTOS / sistema operacional    */
#define SYS_LIGHTWEIGHT_PROT 0 /* sem proteção de seção crítica     */
#define MEM_LIBC_MALLOC 0      /* usa heap interno do lwIP          */

/* ── Memória ────────────────────────────────────────────── */
#define MEM_ALIGNMENT 4 /* alinhamento ARM   */
#define MEM_SIZE 4000   /* heap lwIP (bytes) */
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_ARP_QUEUE 10
#define PBUF_POOL_SIZE 24
#define PBUF_POOL_BUFSIZE 1536

/* ── ARP ────────────────────────────────────────────────── */
#define LWIP_ARP 1
#define ARP_TABLE_SIZE 10
#define ARP_QUEUEING 1

/* ── IP ─────────────────────────────────────────────────── */
#define LWIP_IPV4 1
#define IP_FORWARD 0
#define IP_REASSEMBLY 1
#define IP_FRAG 1
#define IP_REASS_MAX_PBUFS 10
#define IP_DEFAULT_TTL 255

/* ── ICMP ───────────────────────────────────────────────── */
#define LWIP_ICMP 1

/* ── DHCP ───────────────────────────────────────────────── */
#define LWIP_DHCP 1

/* ── UDP ────────────────────────────────────────────────── */
#define LWIP_UDP 1
#define LWIP_UDPLITE 0
#define UDP_TTL 255

/* ── TCP ────────────────────────────────────────────────── */
#define LWIP_TCP 1
#define TCP_TTL 255
#define TCP_WND (4 * TCP_MSS)
#define TCP_MSS 1460
#define TCP_SND_BUF (4 * TCP_MSS)
#define TCP_SND_QUEUELEN ((4 * TCP_SND_BUF) / TCP_MSS)
#define MEMP_NUM_TCP_PCB 5
#define MEMP_NUM_TCP_PCB_LISTEN 5
#define TCP_MAXRTX 12
#define TCP_SYNMAXRTX 4

/* ── Callbacks de socket / netconn (desativados em NO_SYS) ─ */
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0

/* ── DNS ────────────────────────────────────────────────── */
#define LWIP_DNS 1
#define DNS_MAX_SERVERS 2

/* ── Checksum: calculados por software ─────────────────── */
#define CHECKSUM_BY_HARDWARE 0

/* ── Interface de rede ──────────────────────────────────── */
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1

/* ── Timers (necessário para DHCP e TCP) ───────────────── */
#define LWIP_TIMERS 1

/* ── Debug (desative em produção) ──────────────────────── */
#define LWIP_DEBUG 0
#define TCP_DEBUG LWIP_DBG_OFF
#define DHCP_DEBUG LWIP_DBG_OFF
#define IP_DEBUG LWIP_DBG_OFF

/* ── Estatísticas (opcional) ────────────────────────────── */
#define LWIP_STATS 0

/* ── Compatibilidade com pico-sdk / CYW43 ───────────────── */
#define LWIP_CHKSUM_ALGORITHM 3

#endif /* _LWIPOPTS_H */