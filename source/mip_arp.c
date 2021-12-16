#include "../headers/mip.h"
#include "../headers/mip_arp.h"
#include "../headers/mip_debug.h"
#include "../headers/utils.h"
#include "../headers/queue.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>             /* memcpy, memcmp */
#include <ifaddrs.h>
#include <arpa/inet.h>

#include <sys/socket.h>         /* sockaddr_ll */
#include <linux/if_packet.h>    /* sockaddr_ll */
#include <net/ethernet.h>       /* sockaddr_ll */

arp_entry* add_entry(arp_entry **entries, uint8_t mip_address, 
    uint8_t mac_addr[], uint8_t interface[], int ifi)
{
    arp_entry *new_entry;
    uint8_t local[] = LOCAL;
    int i;

    new_entry = allocate_memory(sizeof(struct arp_entry));
    if (new_entry == NULL)
        return NULL;

    new_entry -> mip_address = mip_address;
    memcpy(new_entry -> dest_mac_addr, mac_addr, MAC_ADDR_LEN);

    /* copying only if the mac address is not local */
    if (interface != local)
        memcpy(new_entry -> interface, interface, MAC_ADDR_LEN);

    new_entry -> ifindex = ifi;

    for (i = 0; i < MAX_TABLE_SIZE; i++)
    {
        if (entries[i] == NULL)
        {
            entries[i] = new_entry;
            return new_entry;
        }
    }

    fprintf(stderr, "arp table is full");
    return NULL;
}

int get_arp_entry_by_mip_address(arp_entry **entries, struct arp_entry *dest, uint8_t mip_address)
{
    int i;
    for (i = 0; i < MAX_TABLE_SIZE && entries[i]; i++)
    {
        if (entries[i] -> mip_address == mip_address)
        {
            memcpy(dest, entries[i], sizeof(struct arp_entry));
            return 0;
        }
    }

    return 1;
}

int get_arp_entry_by_sll_ifindex(arp_entry **entries, struct arp_entry *dest, int ifi)
{
    int i;
    for (i = 0; i < MAX_TABLE_SIZE && entries[i]; i++)
    {       
        if (entries[i] -> ifindex == ifi)
        {
            memcpy(dest, entries[i], sizeof(arp_entry));
            return 0;
        }
    }
    
    return 1;
}
            
int remove_entry(arp_entry **entries, arp_entry *entry)
{
    int i;
    arp_entry *e = NULL;
    for (i = 0; i < MAX_TABLE_SIZE && entries[i]; i++)
    {
        if (entries[i] == NULL) return 1;

        if (entries[i] == entry)
        {
            e = entries[i];

            /* swapping latter entries forward */
            while (entries[i] != NULL)
            {
                entries[i] = entries[i + 1];
                i++;
            }
            break;
        }
    }

    if (e == NULL)
    {
        fprintf(stderr, "<daemon>: entry is not in table...");
        return -1;
    }

    free(e);
    return 0;
}

void free_arp_table(arp_entry **entries)
{
    int i;
    for (i = 0; i < MAX_TABLE_SIZE; i++)
    {
        if (entries[i] == NULL)
        {
            free(entries);
            return;
        } 
        free(entries[i]);
    }
    
}

int send_arp_request(ifs *ifs, uint8_t req_addr, uint8_t src)
{
    struct mip_arp_sdu  mip_arp_sdu = {0};
        
    mip_arp_sdu.type        = ARP_REQ;
    mip_arp_sdu.address     = req_addr;
    mip_arp_sdu.padding     = 0;

    return mip_broadcast(ifs, src, MIP_ARP, &mip_arp_sdu, sizeof(struct mip_arp_sdu));
}

int send_arp_response(ifs *ifs, arp_entry **entries, 
    mip_arp_sdu mip_arp_sdu, frame_header frame_header, uint8_t dest_mip_addr)
{
    size_t                      iovlen = 3;
    struct msghdr               *msg;
    struct iovec                msgvec[iovlen];
    struct mip_pdu              mip_pdu;
    struct sockaddr_ll          so_name = {0};
    struct arp_entry            arp_entry;

    mip_arp_sdu.type = ARP_RES;

    mip_pdu.dest = dest_mip_addr;
    mip_pdu.src = ifs -> src_mip_addr;
    mip_pdu.ttl = DEFAULT_TTL;
    mip_pdu.sdu_len = sizeof(struct mip_arp_sdu);
    mip_pdu.sdu_type = MIP_ARP;

    msgvec[0].iov_base = &frame_header;
    msgvec[0].iov_len = sizeof(struct frame_header);
    msgvec[1].iov_base = &mip_pdu;
    msgvec[1].iov_len = sizeof(struct mip_pdu);
    msgvec[2].iov_base = &mip_arp_sdu;
    msgvec[2].iov_len = sizeof(struct mip_arp_sdu);

    msg = allocate_memory(sizeof(struct msghdr));
    if (msg == NULL)
        return -1;

    get_arp_entry_by_mip_address(entries, &arp_entry, dest_mip_addr);
    get_interface_on_ifindex(ifs, &so_name, arp_entry.ifindex);
    
    msg -> msg_name     = &so_name;
    msg -> msg_namelen  = sizeof(struct sockaddr_ll);
    msg -> msg_iovlen   = iovlen;
    msg -> msg_iov      = msgvec;

    if (sendmsg(ifs -> raw_socket, msg, 0) <= 0)
    {
        printf("%s\n", __FUNCTION__);
        perror("sendmsg");
        free(msg);
        return -1;
    }

    free(msg);
    return 0;
}


