#include "../headers/mip_daemon.h"
#include "../headers/mip_routing.h"
#include "../headers/mip.h"
#include "../headers/mip_arp.h"
#include "../headers/mip_debug.h"
#include "../headers/utils.h"
#include "../headers/common.h"
#include "../headers/queue.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

int main(int argc, char* argv[])
{
    int HELP = 0;
    int DEBUG = 0;
    int socket_index = 1, addr_index = 2, c, rc, wc;
    int upper_fd, lower_fd, epoll_fd, app_fd, routing_fd, tmp_fd;
    char                        *unix_socket_name;
    char                        entity_type_identifier;
    char                        buf[MAX_MSG_SIZE];
    uint8_t                     mip_address, addr_ptr;
    uint8_t                     local[MAC_ADDR_LEN] = LOCAL;
    struct mip_sdu              *sdu;
    struct queue                *pkt_queue;
    struct queue_entry          *qe;
    struct pkt_buf_entry        *pkt_buf_entry;
    struct mip_pdu              *pdu;
    struct network_interfaces   *ifs;
    struct arp_entry            **arp_table, *arp_entry;
    struct epoll_event          events_struct = {0}, events[MAX_EVENTS] = {0};

    upper_fd = lower_fd = epoll_fd = app_fd = routing_fd = tmp_fd = -1;

    if (argc < 3 || argc > 5)
    {
        printf("%s\n", "usage: ./mip_daemon [-h] [-d] <socket_upper> <mip_address>");
        return EXIT_SUCCESS;
    }

    while ((c = getopt(argc, argv, "hd")) != -1)
    {
        switch (c)
        {
            case 'h':
                HELP = 1;
                break;
            case 'd':
                DEBUG = 1;
                socket_index = 2;
                addr_index = 3;
                break;
            default:
                break;
        }
    }

    if (HELP) {
        printf("%s\n", "-h >> usage: ./mip_daemon [-h] [-d] <socket_upper> <mip_address>");
        return EXIT_SUCCESS;
    }

    /* get command line arguments */
    unix_socket_name = argv[socket_index];
    mip_address = atoi(argv[addr_index]);

    if (!in_range(mip_address, MIN_MIP_ADDR, MAX_MIP_ADDR))
    {
        printf("%s {%d...%d} (%d)\n", "mip address must be in range", MIN_MIP_ADDR, MAX_MIP_ADDR, mip_address);
        return EXIT_SUCCESS;
    }

    /* create daemon */
    if (daemon(NO_CHANGE, NO_CHANGE) == -1)
    {
        perror("daemon");
        return EXIT_FAILURE;
    }

    /* set up listening on local socket comms */
    upper_fd = prepare_unix_socket(unix_socket_name);
    if (upper_fd == -1)
    {
        return EXIT_FAILURE;
    }
        
    /* get lower layer socket */
    lower_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));
    if (lower_fd == -1)
    {
        perror("socket");
        close(upper_fd);
        return EXIT_FAILURE;
    }

    ifs = allocate_memory(sizeof(struct network_interfaces));
    if (ifs == NULL)
    {
        close(upper_fd); close(lower_fd);
        return EXIT_FAILURE;
    }

    arp_table = allocate_memory(sizeof(arp_entry) * MAX_TABLE_SIZE); /* must also free */
    if (arp_table == NULL)
    {
        free(ifs);
        close(upper_fd); close(lower_fd);
        return EXIT_FAILURE;
    }

    if (get_mac_from_interface(ifs) == -1)
    {
        free(ifs); free_arp_table(arp_table);
        close(upper_fd); close(lower_fd);
        return EXIT_FAILURE;
    }

    ifs -> raw_socket = lower_fd;
    ifs -> src_mip_addr = mip_address;

    /* get first entry to arp table */
    arp_table[0] = add_entry(arp_table, mip_address, (uint8_t*) ifs -> addr[0].sll_addr, 
        local, ifs -> addr[0].sll_ifindex);
    if (arp_table[0] == NULL)
    {
        free(ifs); free_arp_table(arp_table);
        close(upper_fd); close(lower_fd);
        return EXIT_FAILURE;
    }

    /* add rest of local ifs */
    for (c = 1; c < ifs -> ifs_size; c++)
    {
        arp_entry = add_entry(arp_table, mip_address, (uint8_t*) ifs -> addr[c].sll_addr, 
            local, ifs -> addr[c].sll_ifindex);
        if (arp_entry == NULL)
        {
            free(ifs); free_arp_table(arp_table);
            close(upper_fd); close(lower_fd);
            return EXIT_FAILURE;
        }
    }

    epoll_fd = epoll_create(1);
    if (epoll_fd == -1) /* argument is ignored in Linux 2.6.8 */
    {
        perror("epoll_create");
        free(ifs); free_arp_table(arp_table);
        close(upper_fd); close(lower_fd);
        return EXIT_FAILURE;
    }

    /* add upper layer socket to table of sockets of interest */
    rc = epoll_add_to_table(epoll_fd, &events_struct, upper_fd);
    if (rc == -1)
    {
        free(ifs); free_arp_table(arp_table);
        close(upper_fd); close(lower_fd); close(epoll_fd);
        return EXIT_FAILURE;
    }

    /* add lower layer socket to table of sockets of interest */
    rc = epoll_add_to_table(epoll_fd, &events_struct, lower_fd);
    if (rc == -1)
    {
        free(ifs); free_arp_table(arp_table);
        close(upper_fd); close(lower_fd); close(epoll_fd);
        return EXIT_FAILURE;
    }

    pkt_queue = queue_create();
    if (pkt_queue == NULL)
    {
        free(ifs); free_arp_table(arp_table);
        close(upper_fd); close(lower_fd); close(epoll_fd);
        return EXIT_FAILURE;
    }

    do
    {       
        rc = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        /* error */
        if (rc == -1)
        {
            perror("epoll_wait");
            free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
            queue_flush(pkt_queue);
            close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
            return EXIT_FAILURE;
        }

        /* someone is trying to connect through the unix socket */
        else if (events -> data.fd == upper_fd) 
        {
            tmp_fd = handle_connection_request(upper_fd, epoll_fd, events_struct);
            if (tmp_fd == -1)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }
        }

        /* handle entity type indentifier from upper layers */
        else if (events -> data.fd == tmp_fd)
        {
            rc = read(tmp_fd, &entity_type_identifier, sizeof(char));
            if (rc == -1)
            {
                fprintf(stderr, "%s() ", __FUNCTION__);
                perror("read");
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }

            if (atoi(&entity_type_identifier) == MIP_PING) 
            {
                app_fd = tmp_fd;  
            } 

            else if (atoi(&entity_type_identifier) == MIP_ROUTING)
            {
                routing_fd = tmp_fd;
            }

            tmp_fd = -1;
        }

        /* handle incoming packet from routing daemon */
        else if (events -> data.fd == routing_fd)
        {
            rc = mip_routing_recv(routing_fd, buf);

            /* error */
            if (rc == -1)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }

            /* read returned 0 bytes, socket must be closed on other end */
            if (rc == -2)
            {
                rc = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, app_fd, &events_struct);
                if (rc == -1)
                {
                    perror("epoll_ctl");
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }
                close(routing_fd);
                continue;
            }

            /* if we received a hello packet, broadcast it */
            if (rc == 1)
            {
                rc = mip_broadcast(ifs, mip_address, MIP_ROUTING, buf, HEL_SIZE);
                if (rc == -1)
                {
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }
            }

            /* if we received an update packet, unicast it */
            else if (rc == 2)
            {
                /* unicast here */
                pdu = mip_get_pdu(buf[0], mip_address, DEFAULT_TTL, UPD_SIZE + 3 * buf[6], MIP_ROUTING);
                if (pdu == NULL)
                {
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                wc = mip_link_send(arp_table, ifs, pdu, buf, pdu->sdu_len + 2, DEBUG);
                if (wc == -1)
                {
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue); free(pdu);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                free(pdu);

            }

            /* if we get a lookup response */
            else if (rc == 3)
            {
                sdu = allocate_memory(sizeof(struct mip_sdu));
                if (sdu == NULL)
                {
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                sdu->payload = allocate_memory(MAX_RT_PKT_SIZE);
                if (sdu->payload == NULL)
                {
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                mip_deserialize_sdu(buf, sdu, RES_SIZE);
                if (DEBUG) 
                {
                    printf("<daemon>: received lookup response:\n");
                    mip_print_sdu(sdu, MIP_ROUTING);
                }

                /* if we couldn't match the mip address in the routing table */
                if ((uint8_t) sdu->payload[3] == MAX_MIP_ADDR)
                {
                    // qe = get_entry_by_mip_addr(pkt_queue, sdu->dest);
                    qe = pkt_queue->tail;
                    free(sdu->payload); free(sdu);
                    sdu = (struct mip_sdu*) ((struct pkt_buf_entry*) qe->data)->sdu;
                    pdu = (struct mip_pdu*) ((struct pkt_buf_entry*) qe->data)->pdu;
                    if (DEBUG) 
                    {
                        printf("<daemon>: no route to destination host was found. Destroying packets:\n");
                        mip_print_sdu(sdu, MIP_PING);
                        mip_print_pdu(pdu);
                    }
                    free(pdu);
                    free(sdu->payload);
                    free(sdu);
                    queue_entry_destroy(pkt_queue, qe);
                    continue;
                }

                /* else, send the buffered packet */
                // qe = get_entry_by_mip_addr(pkt_queue, sdu->payload[3]);
                qe = pkt_queue->tail;
                if (qe == NULL) continue;
                pdu = (struct mip_pdu*) ((struct pkt_buf_entry*) qe->data)->pdu;
                pdu->dest = sdu->payload[3];
                free(sdu->payload); free(sdu);
                sdu = (struct mip_sdu*) ((struct pkt_buf_entry*) qe->data)->sdu;
                sdu->ttl = pdu->ttl;

                if (DEBUG) 
                {
                    printf("<daemon>: got PDU and SDU from packet buffer to be sent:\n");
                    mip_print_sdu(sdu, MIP_PING);
                    mip_print_pdu(pdu);
                }

                mip_serialize_sdu(buf, sdu);

                /* forward sdu to to link layer */
                wc = mip_link_send(arp_table, ifs, pdu, buf, pdu->sdu_len + 2, DEBUG);
                if (wc == -1)
                {
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                /* we sent arp request, caching packet in buffer again until destination mac address is received */
                else if (wc == 1)
                {   
                    continue;
                }

                /* we finally sent the packet */
                else
                {
                    free(pdu);
                    free(sdu->payload);
                    free(sdu);
                    queue_entry_destroy(pkt_queue, qe);
                }
            }
        }

        /* handle incoming packet from application layer */
        else if (events -> data.fd == app_fd) 
        {
            sdu = allocate_memory(sizeof(struct mip_sdu));
            if (sdu == NULL)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }

            /* read from upper layer socket */
            rc = mip_app_recv(app_fd, sdu);
            if (rc == -1)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }
            
            /* unix socket closed on other end */
            else if (rc == -2)
            {
                rc = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, app_fd, &events_struct);
                if (rc == -1)
                {
                    perror("epoll_ctl");
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }
                close(app_fd);
                free(sdu->payload); free(sdu);
                continue;
            }

            wc = mip_send_routing_lookup_request(routing_fd, mip_address, sdu->dest, DEBUG);
            if (wc == -1)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }

            pdu = mip_get_pdu(sdu->dest, mip_address, sdu->ttl, strlen(sdu->payload), MIP_PING);
            if (pdu == NULL)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }

            pkt_buf_entry = allocate_memory(sizeof(struct pkt_buf_entry));
            if (pkt_buf_entry == NULL)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }

            /* caching packet until we get a lookup response */
            if (DEBUG) 
            {
                printf("<daemon>: caching packet in buffer:\n");
                mip_print_sdu(sdu, MIP_PING);
                mip_print_pdu(pdu);
            }
            pkt_buf_entry->pdu = pdu;
            pkt_buf_entry->sdu = sdu;
            queue_head_push(pkt_queue, pkt_buf_entry);
        }

        /* handle incoming frame from lower layer */
        else if (events -> data.fd == lower_fd) 
        {

            pdu = allocate_memory(sizeof(struct mip_pdu));
            if (pdu == NULL)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }

            /* get packet from link layer socket */
            rc = mip_link_recv(arp_table, ifs, pdu, buf, &addr_ptr, DEBUG);

            /* error */
            if (rc == -1)
            {
                free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                queue_flush(pkt_queue);
                close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                return EXIT_FAILURE;
            }

            if (rc < 3)
            {
                sdu = allocate_memory(sizeof(struct mip_sdu));
                if (sdu == NULL)
                {
                    fprintf(stderr, "<daemon>: error at %d in %s\n", __LINE__, __FUNCTION__);
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                sdu->payload = allocate_memory(pdu->sdu_len);
                if (sdu->payload == NULL)
                {
                    fprintf(stderr, "<daemon>: error at %d in %s\n", __LINE__, __FUNCTION__);
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }
                mip_deserialize_sdu(buf, sdu, pdu->sdu_len + 2);
            }

            /* forward packet to application layer */
            if (rc == 0)
            {
                sdu->dest = pdu->src; /* switching dest and src address */

                wc = mip_app_send(app_fd, sdu);
                if (wc == -1)
                {
                    fprintf(stderr, "<daemon>: error at %d in %s\n", __LINE__, __FUNCTION__);
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                free(pdu); free(sdu->payload); free(sdu);
            }

            /* forwarding packet, do routing lookup first */
            else if (rc == 1)
            {
                /* check if time-to-live has expired */
                if (--pdu->ttl == 0) {
                    if (DEBUG) 
                    {
                        printf("<daemon>: PDU time-to-live expired\n");
                    }
                    free(pdu); free(sdu->payload); free(sdu);
                    continue;
                } 

                if (DEBUG) 
                {
                    printf("<daemon>: forwarding this packet:\n");
                    mip_print_sdu(sdu, MIP_PING);
                }

                wc = mip_send_routing_lookup_request(routing_fd, mip_address, sdu->dest, DEBUG);
                if (wc == -1)
                {
                    fprintf(stderr, "<daemon>: error at %d in %s\n", __LINE__, __FUNCTION__);
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                if (DEBUG) 
                {
                    printf("<daemon>: did a lookup to forward packet, pushing it to queue instead.\n");
                }

                pkt_buf_entry = allocate_memory(sizeof(struct pkt_buf_entry));
                if (pkt_buf_entry == NULL)
                {
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                pkt_buf_entry->sdu = sdu;
                pkt_buf_entry->pdu = pdu;
                queue_head_push(pkt_queue, pkt_buf_entry);
            }

            /* SDU is a routing packet */
            else if (rc == 2)
            {
                wc = write(routing_fd, buf, pdu->sdu_len);
                if (wc == -1)
                {
                    fprintf(stderr, "<daemon>: error at %d in %s()\n", __LINE__, __FUNCTION__);
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                free(pdu); 
                free(sdu->payload); 
                free(sdu);
            }

            /* if received msg is an arp response, we can send packets from outgoing buffer */
            else if (rc == 3)
            {        
                free(pdu); 
                // qe = get_entry_by_mip_addr(pkt_queue, addr_ptr);
                qe = pkt_queue->tail;
                if (qe == NULL) continue;
                sdu = (struct mip_sdu*) ((struct pkt_buf_entry*) qe->data)->sdu;
                pdu = (struct mip_pdu*) ((struct pkt_buf_entry*) qe->data)->pdu;
                mip_serialize_sdu(buf, sdu);

                wc = mip_link_send(arp_table, ifs, pdu, buf, pdu->sdu_len + 2, DEBUG);
                if (wc == -1)
                {
                    free(ifs); free_arp_table(arp_table); free(pkt_buf_entry); free_pkt_buffer(pkt_queue); 
                    queue_flush(pkt_queue);
                    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
                    return EXIT_FAILURE;
                }

                free(pdu);
                free(sdu->payload);
                free(sdu);
                queue_entry_destroy(pkt_queue, qe);                
                continue;
            }

            /* if received msg was an ARP request, this was handled by mip_link_recv */
            else if (rc == 4)
            {
                free(pdu);
                continue;
            } 

        }

    } while (1);

    printf("<daemon>: user interruption\n");

    /* for debugging memory leaks */
    free(ifs); 
    free_arp_table(arp_table); 
    free(pkt_buf_entry); 
    free_pkt_buffer(pkt_queue); 
    queue_flush(pkt_queue);
    close(upper_fd); close(lower_fd); close(epoll_fd); close(app_fd); close(routing_fd);
    return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

queue_entry* get_entry_by_mip_addr(struct queue *q, uint8_t addr)
{
    size_t len = queue_length(q);
    struct queue_entry *qe = q->head;
    struct mip_sdu *e;
    int i;

    for (i = 0; i < (int) len; i++)
    {
        e = (mip_sdu*) qe->data;
        if (e->dest == addr)
        {
            return qe;
        } 
        qe = qe->next;
    }
    fprintf(stderr, "[WARNING]: %s() at line %d, did not return a packet from buffer\n", __FUNCTION__, __LINE__);
    return NULL; /* this may happen if we get multiple ARP responses */
}

void free_pkt_buffer(queue *pkt_buf)
{
    int i;
    int len = (int) queue_length(pkt_buf);
    struct queue_entry *qe = pkt_buf->head;
    struct mip_sdu *sdu;
    struct mip_pdu *pdu;

    for (i = 0; i < (int) len; i++)
    {
        sdu = (struct mip_sdu*) ((struct pkt_buf_entry*) qe->data)->sdu;
        pdu = (struct mip_pdu*) ((struct pkt_buf_entry*) qe->data)->pdu;
        free(sdu->payload); free(sdu); free(pdu);
        
        qe = qe->next;
    }
}