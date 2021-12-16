#include "../headers/mip_arp.h"
#include "../headers/mip_debug.h"
#include "../headers/mip.h"
#include "../headers/mip_routing.h"

#include <stdio.h>
#include <string.h>

void mip_print_pdu(mip_pdu *pdu) 
{
    char *arp = "MIP ARP\0", *ping = "PING\0", *routing = "ROUTING\0";
    char *type;
    if (pdu -> sdu_type == MIP_ARP)         type = arp;
    if (pdu -> sdu_type == MIP_PING)        type = ping;
    if (pdu -> sdu_type == MIP_ROUTING)     type = routing;
    printf("\n%30s\n", "--- MIP PDU START ---");
    printf("%24s %d\n", "Destination:", pdu -> dest);
    printf("%24s %d\n", "Source:", pdu -> src);
    printf("%24s %d\n", "Time-to-live:", pdu -> ttl);
    printf("%24s %d\n", "SDU length:", pdu -> sdu_len);
    printf("%24s %s\n", "SDU type:", type);
    printf("%29s\n\n", "--- MIP PDU END ---");
}

void mip_print_frame(frame_header *header)
{
    printf("\n%35s\n", "--- FRAME HEADER START ---");
    printf("%30s","Destination MAC address: ");
    print_mac_address(header -> dest, MAC_ADDR_LEN);
    printf("%30s", "Source MAC address: ");
    print_mac_address(header -> src, MAC_ADDR_LEN);
    printf("%30s %#02x", "Protocol: ", header -> eth_proto[0]);
    printf("%02x\n", header -> eth_proto[1]);
    printf("%34s\n\n", "--- FRAME HEADER END ---");
}

void mip_print_sdu(mip_sdu *sdu, int type)
{
    if (type == MIP_PING)
    {
        printf("\n%30s\n", "--- MIP SDU START ---");
        printf("%20s %d\n", "Destination:", sdu -> dest);
        printf("%20s %d\n", "TTL:", sdu->ttl);
        printf("%7s%s%s\n", "\"", sdu -> payload, "\"");
        printf("%29s\n\n", "--- MIP SDU END ---");
    }

    else if (type == MIP_ROUTING)
    {
        mip_print_routing_sdu(sdu);
    }
}

void mip_print_routing_sdu(struct mip_sdu *sdu) 
{
    uint8_t i;
    char type[4];
    char* lines = "---------------------";
    char *can_reach = "dest", *via = "via", *hops = "hops";
    memcpy(type, sdu->payload, 3);
    type[3] = '\0';

    printf("\n%35s\n", "--- MIP ROUTING SDU START ---");

    if (!strncmp(type, HELLO, 3))
    {
        printf("%22s %s\n", "Type: ", "HELLO");
        printf("%22s %d\n", "Dest: ", sdu->dest);
        printf("%22s %d\n", "TTL: ", sdu->ttl);
        printf("%22s %d\n", "Source: ", sdu->payload[3]);
    }          

    else if (!strncmp(type, UPDATE, 3))
    {
        printf("%22s %s\n", "Type: ", "UPDATE");
        printf("%22s %d\n", "Dest: ", sdu->dest);
        printf("%22s %d\n", "TTL: ", sdu->ttl);
        printf("%22s %d\n", "Length: ", sdu->payload[4]);
        printf("%4s %25s\n", "", lines);
        printf("%9s | %s | %s | %s |\n", "", can_reach, via, hops);
        printf("%4s %25s\n", "", lines);

        for (i = 0; i < (int) sdu->payload[4]; i++)
        {
            printf("%8s | %4d | %3d | %4d |\n", "", (uint8_t) sdu->payload[i * 3 + 5], (uint8_t) sdu->payload[i * 3 + 6], (uint8_t) sdu->payload[i * 3 + 7]);
            printf("%4s %25s\n", "", lines);
        }
    }

    else if (!strncmp(type, REQUESTPKT, 3))
    {
        printf("%22s %s\n", "Type: ", "REQUEST");
        printf("%22s %d\n", "Dest: ", sdu->dest);
        printf("%22s %d\n", "TTL: ", sdu->ttl);
        printf("%22s %d\n", "Requesting: ", sdu->payload[3]);
    } 

    else if (!strncmp(type, RESPONSEPKT, 3))
    {
        printf("%22s %s\n", "Type: ", "RESPONSE");
        printf("%22s %d\n", "Dest: ", sdu->dest);
        printf("%22s %d\n", "TTL: ", sdu->ttl);
        if ((uint8_t) sdu->payload[3] == MAX_MIP_ADDR)
            printf("%27s\n", "No path found");
        else printf("%22s %d\n", "Responding with: ", sdu->payload[3]);
    }

    else printf("%30s\n", "undefined packet type");

    printf("%34s\n\n", "--- MIP ROUTING SDU END ---");
}

void mip_print_routing_table(struct queue *routing_table)
{
    int i;
    size_t len = queue_length(routing_table);
    char* lines = "-------------------------";
    char *num = "#", *can_reach = "dest", *via = "via", *hops = "hops";

    struct queue_entry *qe;
    struct mip_routing_table_entry *e;

    printf("\n%11s %20s\n", "", "MIP ROUTING TABLE");
    printf("%10s %s\n", "", lines);
    printf("%10s | %s | %s | %s | %s |\n", "", num, can_reach, via, hops);
    printf("%10s %s\n", "", lines);

    qe = routing_table->head;

    for (i = 0; i < (int) len; i++)
    {
        e = (struct mip_routing_table_entry*) qe->data;
        printf("%10s | %d | %4d | %3d | %4d |\n", "", i, e->dest, e->next_hop, e->hops);
        printf("%10s %25s\n", "", lines);
        qe = qe->next;
    }
}

void mip_debug(arp_entry **arp_table, 
    uint8_t *src_mac, uint8_t *dest_mac, 
    uint8_t src_mip, uint8_t dest_mip)
{
    printf("%-25s: ", "Source MAC address");
    print_mac_address(src_mac, 6);
    printf("%-25s: ", "Destination MAC address");
    print_mac_address(dest_mac, 6);
    printf("%-25s: %d\n", "Source MIP address", src_mip);
    printf("%-25s: %d\n", "Destination MIP address", dest_mip);
    mip_print_arp_content(arp_table);
}

void mip_print_arp_content(arp_entry **entries)
{
    int i;
    printf("\n%s\n", "ARP cache content\n");

    if (entries[0] == NULL)
    {
        printf("%s\n", "<daemon>: arp cache is empty...");
        return;
    }

    for (i = 0; i < MAX_TABLE_SIZE && entries[i]; i++)
    {
        printf("%d\n", i);
        mip_print_arp_entry(entries[i]);
    }
}

void mip_print_arp_entry(arp_entry *e)
{
    uint8_t local[MAC_ADDR_LEN] = LOCAL;
    printf("%-18s: %u\n", "MIP address", e -> mip_address);
    printf("%-18s: ", "MAC address");
    print_mac_address(e -> dest_mac_addr, MAC_ADDR_LEN);
    printf("%-18s: ", "Interface address");

    if (memcmp(e -> interface, local, MAC_ADDR_LEN) == 0) printf("%-11s\n", "local");
    else print_mac_address(e -> interface, MAC_ADDR_LEN);
    printf("%-18s: %d\n", "Interface index", e -> ifindex);
    printf("\n");
}

void mip_print_arp_packet(mip_arp_sdu sdu)
{
    char *req = "request\0", *res = "response\0";
    char *c = sdu.type == ARP_REQ ? req : res;
    printf("\n%25s\n", " --- ARP packet --- ");
    printf("%14s: %s\n", "Type", c);
    printf("%14s: %u\n", "Address", sdu.address);
    printf("%25s\n\n", " --- ARP packet --- ");
}

void mip_print_packet_queue(struct queue *q)
{
    int i;
    size_t len = queue_length(q);

    struct queue_entry *qe;
    struct mip_sdu *e;

    printf("%20s (%ld)\n", "PACKET QUEUE", len);

    qe = q->head;

    for (i = 0; i < (int) len; i++)
    {
        e = (struct mip_sdu*) qe->data;
        printf("%d\n", i);
        mip_print_sdu(e, MIP_PING);
        qe = qe->next;
    }
}