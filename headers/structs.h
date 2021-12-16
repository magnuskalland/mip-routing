#ifndef STRUCTS_H
#define STRUCTS_H

#include <stddef.h>             /* size_t */
#include <stdint.h>             /* uint8_t */
#include <sys/types.h>
#include <sys/socket.h>         /* sockaddr_ll */
#include <linux/if_packet.h>    /* sockaddr_ll */
#include <net/ethernet.h>       /* sockaddr_ll */

#define MAC_ADDR_LEN        6
#define MAX_IFS             8

/**
 * Structure to represent the payload from the application layer.
 * @param dest Destination address of the packet.
 * @param payload   Payload of the packet.
 * */
typedef struct mip_sdu {
    uint8_t     dest;
    uint8_t     ttl;
    char*       payload;
} mip_sdu;

/**
 * Structure to represent the header of MIP packets
 * @param dest     MIP address of destination
 * @param src      MIP address of source
 * @param ttl           Number of hops before packet destruction
 * @param sdu_len       Payload length in bytes
 * @param sdu_type      0x01 for MIP ARP or 0x02 for Ping
*/
typedef struct mip_pdu {
    uint8_t dest;
    uint8_t src;
    uint8_t ttl : 4;
    size_t sdu_len : 9;
    uint8_t sdu_type : 3;
} __attribute__((packed)) mip_pdu;

/**
 * Structure to represent the frame for the link layer.
 * @param dest  Destination MAC address.
 * @param src  Source MAC address.
 * @param eth_proto Link layer protocol. Should be set to ETH_P_MIP.
 * */
typedef struct frame_header {
    uint8_t dest[MAC_ADDR_LEN];
    uint8_t src[MAC_ADDR_LEN];
    uint8_t eth_proto[2];
} __attribute__((packed)) frame_header;

typedef struct mip_arp_sdu {
    uint type : 1;
    uint8_t address;
    uint padding : 23;
} __attribute__((packed)) mip_arp_sdu;

/**
 * Structure for storing MIP ARP entries.
 * 
 * @param mip_address   MIP address of the node.
 * @param dest_mac_addr MAC address of the node.
 * @param interface     MAC address of the network interface we reach the node on.  
 *                      Is NULL if the MIP address is of a local address.
 * */
typedef struct arp_entry {
    uint8_t mip_address;
    uint8_t dest_mac_addr[MAC_ADDR_LEN];
    uint8_t interface[MAC_ADDR_LEN];
    int     ifindex;
} arp_entry;

/**
 * Structure for storing local network interfaces.
 * @param addr          An array of interfaces.
 * @param src_mip_addr  The source MIP address.
 * @param raw_socket    A raw socket for lower layers.
 * @param ifs_size      Number of interfaces in addr.
*/
typedef struct network_interfaces {
    struct sockaddr_ll  addr[MAX_IFS];
    uint8_t             src_mip_addr;
    int                 raw_socket;
    ssize_t             ifs_size;
} ifs;

#endif