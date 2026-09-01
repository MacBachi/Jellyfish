#ifndef JELLYOS_LWIPOPTS_H
#define JELLYOS_LWIPOPTS_H

// Minimal lwIP configuration for JellyOS: raw API only, no sockets, no threads.
// Based on pico-examples pico_w/wifi/lwipopts_examples_common.h, trimmed to what a
// handful of UDP datagrams per second (and later one small HTTP page) need.

#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

#define MEM_LIBC_MALLOC 0 // required by the threadsafe_background arch
#define MEM_ALIGNMENT 4
#define MEM_SIZE 4000
#define MEMP_NUM_UDP_PCB 4
#define MEMP_NUM_TCP_PCB 4
#define MEMP_NUM_TCP_SEG 16
#define MEMP_NUM_SYS_TIMEOUT 8
#define PBUF_POOL_SIZE 8

#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define TCP_MSS 1460
#define TCP_WND (4 * TCP_MSS)
#define TCP_SND_BUF (4 * TCP_MSS)

#define LWIP_DHCP 1 // stations get their lease from the AP jelly
#define LWIP_DNS 1
#define LWIP_IPV4 1
#define LWIP_IPV6 0

#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1
#define LWIP_CHKSUM_ALGORITHM 3
#define DHCP_DOES_ARP_CHECK 0
#define LWIP_DHCP_DOES_ACD_CHECK 0

#define LWIP_STATS 0
#define LWIP_DEBUG 0

#endif
