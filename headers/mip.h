#ifndef MIP_H
#define MIP_H

#include "structs.h"
#include "mip_routing.h"

#define MIP_HEADER_SIZE     0x04
#define MAX_MSG_SIZE        0x0200 // is 2^9 bytes
#define mip_sdu_SIZE    sizeof(char*) + sizeof(char)
#define MIP_PDU_SIZE        sizeof(mip_pdu)

#define ETH_P_MIP           0x88B5
#define BROADCAST_ADDR      {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define BROADCAST_PERM      0x01

#define MAX_MIP_HOSTS       0x0100
#define MIN_MIP_ADDR        0x00
#define MAX_MIP_ADDR        0xFF

#define MAX_TTL             0x0F
#define POISON_TTL          0x10

#define MIP_ARP             0x01
#define MIP_PING            0x02
#define MIP_ROUTING         0x04

#define DEFAULT_TTL         0x00

struct pkt_buf_entry {
    struct mip_sdu *sdu;
    struct mip_pdu *pdu;
};

int mip_broadcast(const struct network_interfaces *ifs, const uint8_t src, const uint8_t sdu_type, 
    void* sdu, const size_t sdu_len);

/**
 * Function that sends a MIP SDU to the socket given by socket.
 * @param socket        The socket to write to.
 * @param sdu           The SDU to send.
 * @return              The number of bytes written, -1 if error.
 * */
int mip_app_send(int socket, mip_sdu *sdu);

/**
 * Receives a packet from the socket.
 * @param socket        The socket to read from.
 * @param dest_sdu      The SDU to store the read packet in.
 * @return              -1 if error, -2 if the client appliaction has 
 *                      disconnected, 0 otherwise.
 * */
int mip_app_recv(int socket, mip_sdu *dest_sdu);

int mip_routing_recv(int socket, char* buf);

/**
 * Function to send a MIP packet to the MIP address given by dest.
 * @param arp_table The entry point to the ARP table of this host.
 * @param ifs       Local interfaces of this host.
 * @param src  The MIP address of this host.
 * @param sdu       The MIP SDU to send.
 * @param debug     Flag to indicate if the function should print debug info.
 * @return          -1 if error, 
 *                  0 if success,
 *                  1 if this function fires off an ARP request. This means 
 *                  the implementation of this protocol should store the SDU
 *                  in a buffer and send when an ARP response is returned.
 * */
int mip_link_send(arp_entry** arp_table, ifs *ifs, mip_pdu *pdu,
    char *sdu, size_t len, int debug);

/**
 * Reads from the link layer socket of this host. Will reject packets that 
 * is not broadcast or targeted for the MIP address of this host. The function
 * will handle ARP packets according to the MIP RFC.
 * @param arp_table The entry point to the ARP table of this host.
 * @param ifs       Local interfaces of this host.
 * @param pdu       The PDU to store the received packet in.
 * @param sdu       A buffer for the to store the received packet in.
 * @param debug     Flag to indicate if the function should print debug info.
 * @return          -1 if error
 *                  0 if the SDU is to be forwared to the application layer.
 *                  1 if the SDU is meant to be forwarded after doing a routing lookup
 *                  2 if the SDU is to be forwarded to the routing application.
 *                  3 if the SDU is of type ARP response.
 *                  4 if the SDU is of type ARP request.
 * */
int mip_link_recv(arp_entry **arp_table, ifs *ifs, mip_pdu *pdu, 
    char *sdu, uint8_t *arp_addr, int debug);

/**
 * Constructs a MIP PDU header from the given arguments.
 * @param dest     The MIP destination address of the packet.
 * @param src      The MIP source address of this host.
 * @param sdu_len       The size of the SDU encapsulated by this header.
 * @param sdu_type      Should be set to either MIP-ARP or PING.
 * @return              NULL if error, the constructed MIP PDU otherwise.
 * */
mip_pdu *mip_get_pdu(uint8_t dest, uint8_t src, uint8_t ttl,
    size_t sdu_len, uint8_t sdu_type);

/**
 * Takes a socket name and creates a file descriptor with socket(), then
 * connects to the other side.
 * @param socket_name   The file for the socket
 * @param entity        A type to identify what application that are connecting
 * @return              -1 if error, the new file descriptor from connect()
 *                      otherwise
 * */
int mip_connect_unix_socket(char *socket_name, char entity);

/**
 * Gets network interfaces of this host. Filters out interfaces that is not
 * of type AF_PACKET and loopback.
 * @param ifs   A structure for storing interfaces.
 * @return      -1 if error, 0 otherwise.
*/
int get_mac_from_interface(ifs *ifs);

/**
 * Finds the sockaddr_ll of the interfaces gotten from get_mac_from_interface() at
 * the given interface index ifi.
 * @param head      The pointer to the interfaces structure populated by get_mac_
 *                  from_interface().
 * @param dest      The sockaddr_ll of the interface.
 * @param ifi       The interface index we are looking for.
 * @return          1 if the interface doesn't not exist in head, 0 if success.
 * */
int get_interface_on_ifindex(ifs *ifs, struct sockaddr_ll *dest, int ifi);

/**
 * Function to serialize the given MIP SDU.
 * @param dest  The destination buffer.
 * @param src   The source MIP SDU.
 * */
void mip_serialize_sdu(char* dest, mip_sdu *src);

/**
 * Function to deserialize a MIP SDU.
 * @param src       The buffer to deserialize.
 * @param dest      The destination MIP SDU.
 * @param src_len   The length of the buffer.
 * */
void mip_deserialize_sdu(char* src, mip_sdu *dest, size_t src_len);

int mip_send_routing_lookup_request(int socket, uint8_t src, uint8_t dest, int debug);
#endif