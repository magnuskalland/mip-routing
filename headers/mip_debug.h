#ifndef MIP_DEBUG_H
#define MIP_DEBUG_H

#include "structs.h"
#include "mip_routing.h"

/**
 * Prints the given SDU in a nicely formatted way.
 * @param sdu       The SDU to print.
 * */
void mip_print_sdu(mip_sdu *sdu, int type);

/**
 * Prints the given PDU header in a nicely formatted way.
 * @param pdu       The PDU to print.
 * */
void mip_print_pdu(mip_pdu *pdu);

/**
 * Prints the given frame in a nicely formatted way.
 * @param frame     The frame to print.
 * */
void mip_print_frame(frame_header *frame);

/**
 * Prints debug information that prints information for each communication 
 * instance.
 * @param head      An entry to the ARP table.
 * @param src_mac   Source MAC address.
 * @param dst_mac   Destination MAC address.
 * @param src_mip   Source MIP address.
 * @param dst_mip   Destination MIP address.
 * */
void mip_debug(arp_entry **arp_table, 
    uint8_t *src_mac, uint8_t *dst_mac, 
    uint8_t src_mip, uint8_t dst_mip);

/**
 * Function that prints a MAC address.
 * @param addr      The MAC address to be printed.
 * @param len       Length of the address. Should be set to MAC_ADDR_LEN.
 * */
void print_mac_address(uint8_t addr[], size_t len);

/**
 * Function for printing a sockaddr_ll struct.
 * @param sockaddr      The structure to print.
 * */
void print_sockaddr_ll(struct sockaddr_ll *sockaddr);

/**
 * Prints the ARP table associated with head
 * @param head      the entry to the ARP table
 * */
void mip_print_arp_content(arp_entry **entries);

/**
 * Prints a single ARP entry
 * @param entry     the entry to print
 * */
void mip_print_arp_entry(arp_entry *entry);

/**
 * Debug function to print an ARP packet.
 * @param sdu       The packet to print.
 * */
void mip_print_arp_packet(mip_arp_sdu sdu);

void mip_print_routing_sdu(struct mip_sdu *sdu);
void mip_print_routing_table(struct queue *routing_table);
void mip_print_packet_queue(struct queue *q);

#endif