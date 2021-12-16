#ifndef MIP_ROUTING_H
#define MIP_ROUTING_H

#include "queue.h"
#include "structs.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

extern int DEBUG;

#define MAX_NTWRK_SIZE          15
#define INFINITY                255

#define HELLO                   "HEL"
#define UPDATE                  "UPD"
#define REQUESTPKT              "REQ"
#define RESPONSEPKT             "RES"

#define HEL_SIZE                0x06
#define UPD_SIZE                0x07        /* plus 3 times length */
#define REQ_SIZE                0x06
#define RES_SIZE                0x06

#define HELLO_TIMEOUT           1

#define LOOKUP_PKT_SIZE         7
#define MAX_RT_PKT_SIZE         UPD_SIZE + 3 * MAX_NTWRK_SIZE + 2

enum states {
    STATE_HELLO,
    STATE_UPDATE,
    STATE_WAIT,
    STATE_EXIT,
};

/**
 * Structure for representing a routing table entry.
 * @param dest          The destination node.
 * @param next_hop      Next node in path.
 * @param hops          Number of hops in path.
 * @param hello_count   For timeout. 
 * */
struct mip_routing_table_entry {
    uint8_t                 dest;
    uint8_t                 next_hop;
    uint8_t                 hops;
    uint8_t                 hello_count;
} mip_routing_table_entry;

/**
 * Function for sending a routing lookup response.
 * 
 * @param socket            Socket to send over.
 * @param routing_table     The routing table of this host.
 * @param src               The source address of this host.
 * @param req               The requested address.
 * @return                  -1 if error, 0 otherwise.
 * */
int send_routing_res(int socket, queue *routing_table, uint8_t src, uint8_t req);

/**
 * Function for updating a routing table entry.
 * @param routing_table     The routing table of this host.
 * @param src               The target node.
 * @param dest              The next hop address.
 * @param hops              Hop count.
 * @param hello_count       The HELLO count to synchronize new entries with.
 * 
 * @return                  1 if the routing table was updated, 
 *                          0 if not, -1 if error.
 * */
int update_entry(queue *routing_table, uint8_t src, uint8_t dest, uint8_t hops, uint8_t hello_count);

/**
 * Function that takes a routing table and merges it with this hosts routing table.
 * @param routing_table     The routing table of this host.
 * @param sdu               The SDU containing the received routing table.
 * @param mip_address       This hosts MIP address.
 * @param hello_count       The HELLO count to synchronize new entries with.
 * @return                  1 if the routing table was updated, 
 *                          0 if not, -1 if error.
 * 
 * */
int update_table(queue * routing_table, struct mip_sdu *sdu, uint8_t mip_address, uint8_t hello_count);

/**
 * Function for sending a hello packet.
 * @param socket    FD to write to.
 * @param src       The MIP address of this host.
 * @return          -1 if error, 0 otherwise.
 * */
int send_hello(int socket, uint8_t src);

/**
 * Function that unicast routing tables to all adjacent hosts except for given host.
 * @param socket            FD to write to.
 * @param routing_table     The routing table of this host.
 * @param last_upd_src      The address of the host that triggered this update.
 * @param src               The MIP address of this host.
 * @return                  -1 if error, 0 otherwise.
 * */
int send_update(int socket, struct queue *routing_table, uint8_t last_upd_src, uint8_t src);

/**
 * Function that receives a routing SDU from underlyding daemon.
 * @param socket    FD to write to.
 * @param packet    The buffer to write to.
 * */
int recv_from_daemon(int socket, mip_sdu *packet);

/**
 * Function that checks for timeouts in routing table.
 * @param routing_table     The routing table of this host.
 * @param hello_count       The HELLO count to detect timeouts with.
 * @return                  A host that time out. NULL if no host timed out.
 * */
struct mip_routing_table_entry *check_for_timeouts(queue *routing_table, uint8_t hello_count);

/**
 * Function that updates hello counts of adjacent nodes.
 * @param routing_table     The routing table of this host.
 * @param src               The MIP address of this host.
 * @param hello_count       The HELLO count to synchronize new entries with.
 * */
void update_hello_count(queue *routing_table, uint8_t src, uint8_t hello_count);

/**
 * Function that frees the given routing table from heap memory.
 * @param routing_table     The table to free.
 * */
void free_routing_table(queue *routing_table);

#endif