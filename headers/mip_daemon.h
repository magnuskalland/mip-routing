#ifndef MIP_DAEMON_H
#define MIP_DAEMON_H

#include "structs.h"
#include "queue.h"
#include <stdint.h>

queue_entry* get_entry_by_mip_addr(struct queue *q, uint8_t addr);
void free_pkt_buffer(queue *pkt_buf);

#endif