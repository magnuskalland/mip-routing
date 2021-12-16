#ifndef MIP_ARP_H
#define MIP_ARP_H

#include "structs.h"
#include <stdint.h>

#define ARP_REQ         0x00
#define ARP_RES         0x01
#define LOCAL           {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define MAX_TABLE_SIZE  16

/**
 * Finds the ARP entry with MIP address mip_address, in the table associated with head, 
 * then memcpy this into dest.
 *
 * @param entries       Entry point to the linked list of ARP entries
 * @param dest          The destination of where to return the found ARP entry
 * @param mip_address   MIP address to look up
 * @return              0 if the entry was found, 1 if not
*/
int get_arp_entry_by_mip_address(arp_entry **entries, struct arp_entry *dest, 
uint8_t mip_address);

/**
 * Finds the ARP entry with MIP address mip_address, in the table associated with head, 
 * then memcpy this into dest.
 * 
 * @param entries       Entry point to the linked list of ARP entries
 * @param dest          The destination of where to return the found ARP entry
 * @param mip_address   Interface index to look up
 * @return              0 if the entry was found, 1 if not
 * */
int get_arp_entry_by_sll_ifindex(arp_entry **entries, struct arp_entry *dest, int ifi);

/**
 * Adds an ARP entry to the linked list given by head. 
 * 
 * @param entries       Entry point to the linked list of ARP entries
 * @param mip_address   The MIP address of the entry
 * @param mac_addr      The MAC address of the entry
 * @param interface     The local interface the MIP should be associated with
 *                      Should be set to 0 if the entry is associated with the 
 *                      local MIP address and one of the local network interfaces
 * @param sll_ifindex   The local interface index of how to reach the MIP host.
 *                      Should be set to -1 if the interface is local
 * @return              The entry that was made, or NULL when error
 * */
struct arp_entry* add_entry(arp_entry **entries, uint8_t mip_address, 
    uint8_t mac_addr[], uint8_t interface[], int ifi);

/**
 * Removes an ARP entry from the linked list given by head.
 * 
 * @param entries   entry to the linked list
 * @param entry     the entry to remove
 * @return          -1 if head is NULL or entry is not in the ll, 0 if success
 * */
int remove_entry(arp_entry **entries, arp_entry *entry);

/**
 * Frees the ARP table from memory given by head.
 * @param entries   The entry to the ARP table to free
 * */
void free_arp_table(arp_entry **entries);

/**
 * Sends an ARP request for the MIP address req_addr. The receiving of the 
 * ARP response should be handled by the application implementing MIP ARP.
 * @param ifs       The local interfaces of the host.
 * @param req_addr  The MIP address to request for.
 * @param src  The MIP address of the host.
 * @return          -1 if error, 0 otherwise.
*/
int send_arp_request(ifs *ifs, uint8_t req_addr, uint8_t src);

/**
 * Sends an ARP response.
 * @param ifs           The local interfaces of this host.
 * @param head          The entry to the ARP table of this host.
 * @param mip_arp_sdu       The ARP SDU packet to send.
 * @param frame_header  The frame header for the link layer.
 * @param dest_mip_addr The MIP address of the destination address.
 * @return              -1 if error, 0 otherwise.
 * */
int send_arp_response(ifs *ifs, arp_entry **entries, 
    mip_arp_sdu mip_arp_sdu, frame_header frame_header, uint8_t dest_mip_addr);
#endif


