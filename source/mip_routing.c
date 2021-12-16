#include "../headers/mip_routing.h"
#include "../headers/mip.h"
#include "../headers/mip_debug.h"
#include "../headers/common.h"
#include "../headers/queue.h"
#include "../headers/utils.h"

#include <stdlib.h>         /* macros */
#include <unistd.h>         /* daemon */
#include <stdio.h>          /* prints */
#include <string.h>         /* memcpy */
#include <sys/timerfd.h>    /* timer fd */

int HELP = 0;
int DEBUG = 0;

int main(int argc, char* argv[])
{
    uint8_t                         mip_address, updated = 0, hello_count = 0;
    int                             sockfd, epollfd, timerfd;
    int                             rc, wc, c, i;
    int                             socket_index = 1, addr_index = 2;
    enum states                     state = STATE_HELLO;
    struct epoll_event              events_struct = {0}, events[MAX_EVENTS] = {0};
    struct itimerspec               timer;
    struct queue_entry              *qe;
    struct mip_routing_table_entry  *e, *e2;
    struct queue                    *routing_table;
    struct mip_sdu                  *sdu;

    if (argc < 3)
    {
        printf("usage: ./mip_routing <socket_lower> <mip_address>\n");
        return EXIT_SUCCESS;
    }

    if (argc < 3 || argc > 5)
    {
        printf("usage: ./mip_routing [-h] [-d] <socket_lower> <mip_address>\n");
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

    mip_address = (uint8_t) atoi(argv[addr_index]);

    /* create daemon */
    if (daemon(NO_CHANGE, NO_CHANGE) == -1)
    {
        perror("daemon");
        return EXIT_FAILURE;
    }

    timer.it_interval.tv_sec = HELLO_TIMEOUT;
	timer.it_interval.tv_nsec = 0;
	timer.it_value.tv_sec = HELLO_TIMEOUT;
	timer.it_value.tv_nsec = 0;

    sockfd = mip_connect_unix_socket(argv[socket_index], MIP_ROUTING + '0');
    if (sockfd == -1)
    {
        fprintf(stderr, " >>> <routing>: did you remember to start the daemon?\n");
        return EXIT_FAILURE;
    }

    timerfd = timerfd_create(CLOCK_REALTIME, 0);
    if (timerfd == -1)
    {
        perror("timerfd_create");
        close(sockfd);
        return EXIT_FAILURE;
    }

    epollfd = epoll_create(1);
    if (epollfd == -1)
    {
        perror("epoll_create");
        close(sockfd); close(timerfd);
        return EXIT_FAILURE;
    }

    rc = epoll_add_to_table(epollfd, &events_struct, sockfd);
    if (rc == -1)
    {
        close(sockfd); close(timerfd); close(epollfd);
        return EXIT_FAILURE;
    }

    rc = epoll_add_to_table(epollfd, &events_struct, timerfd);
    if (rc == -1)
    {
        close(sockfd); close(timerfd); close(epollfd);
        return EXIT_FAILURE;
    }

    routing_table = queue_create();
    if (routing_table == NULL)
    {
        close(sockfd); close(timerfd); close(epollfd);
        return EXIT_FAILURE;
    }

    wc = update_entry(routing_table, mip_address, mip_address, 0, hello_count);
    if (wc == -1)
    {
        close(sockfd); close(timerfd); close(epollfd);
        free_routing_table(routing_table); queue_flush(routing_table);
        return EXIT_FAILURE;
    }

    sdu = allocate_memory(sizeof(struct mip_sdu));
    if (sdu == NULL)
    {
        close(sockfd); close(timerfd); close(epollfd);
        queue_flush(routing_table); free(routing_table);
        return EXIT_FAILURE;
    }

    sdu->payload = allocate_memory(MAX_RT_PKT_SIZE);
    if (sdu->payload == NULL)
    {
        close(sockfd); close(timerfd); close(epollfd);
        free(sdu); free_routing_table(routing_table); 
        queue_flush(routing_table);
        return EXIT_FAILURE;
    }

    do
    {
        memset(sdu->payload, 0, MAX_RT_PKT_SIZE);

        switch(state) {

            case STATE_WAIT:
                rc = epoll_wait(epollfd, events, MAX_EVENTS, -1); /* timeout set by timerfd */
                
                /* error */
                if (rc == -1)
                {
                    close(sockfd); close(timerfd); close(epollfd);
                    free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                    queue_flush(routing_table);
                    return EXIT_FAILURE;
                }

                /* timeout */
                if (events -> data.fd == timerfd)
                {
                    state = STATE_HELLO;
                    updated = 0;
                    while ((e = check_for_timeouts(routing_table, hello_count)) != NULL)
                    {

                        /* setting hosts unreachable over timed out links as unreachable */
                        e->hops = INFINITY;

                        qe = routing_table->head;
                        for (i = 0; i < (int) queue_length(routing_table); i++)
                        {
                            e2 = (struct mip_routing_table_entry*) qe->data;
                            if (e2->next_hop == e->next_hop) e2->hops = INFINITY; 
                            qe = qe->next;
                        }

                        // e->hello_count = hello_count;
                        if (DEBUG) 
                        {
                            printf("<routing>: timeout for node %d\n", e->dest);
                            mip_print_routing_table(routing_table);
                        }
                        updated = 1;
                    }

                    /* because we can't be in two states concurrently, we send update now */
                    if (updated)
                    {
                        /* deliberately sending update, specifying triggering host to be a */
                        /* non-existing host. */
                        wc = send_update(sockfd, routing_table, 255, mip_address);
                        if (wc == -1)
                        {
                            close(sockfd); close(timerfd); close(epollfd);
                            free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                            queue_flush(routing_table);
                            return EXIT_FAILURE;
                        }
                    }

                    break;
                }

                rc = recv_from_daemon(sockfd, sdu);
                if (rc == -1)
                {
                    close(sockfd); close(timerfd); close(epollfd);
                    free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                    queue_flush(routing_table);
                    return EXIT_FAILURE;
                }

                wc = update_table(routing_table, sdu, mip_address, hello_count);   

                if (!strncmp(sdu->payload, HELLO, 3))
                {
                    wc = update_entry(routing_table, sdu->payload[3], sdu->payload[3], 1, hello_count);
                    if (wc == -1)
                    {
                        close(sockfd); close(timerfd); close(epollfd);
                        free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                        queue_flush(routing_table);
                        return EXIT_FAILURE;
                    }

                    update_hello_count(routing_table, sdu->payload[3], hello_count);

                    /* if we did an update to our routing table, propagate update table to adjacent hosts */
                    if (wc)
                    {
                        state = STATE_UPDATE;
                    }
                }

                else if (!strncmp(sdu->payload, UPDATE, 3))
                {    

                    /* if we did an update to our routing table, propagate update table to adjacent hosts */
                    if (wc)
                    {
                        state = STATE_UPDATE;
                    }
                }

                else if (!strncmp(sdu->payload, REQUESTPKT, 3))
                {
                    wc = send_routing_res(sockfd, routing_table, mip_address, sdu->payload[3]);
                    if (wc == -1)
                    {
                        close(sockfd); close(timerfd); close(epollfd);
                        free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                        queue_flush(routing_table);
                        return EXIT_FAILURE;
                    }
                }
                break;

            case STATE_HELLO:
                wc = send_hello(sockfd, mip_address);
                if (wc == -1)
                {
                    close(sockfd); close(timerfd); close(epollfd);
                    free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                    queue_flush(routing_table);
                    return EXIT_FAILURE;
                }

                hello_count++;
                state = STATE_WAIT;

                wc = timerfd_settime(timerfd, 0, &timer, NULL);
                if (wc == -1)
                {
                    perror("timerfd_settime");
                    close(sockfd); close(timerfd); close(epollfd);
                    free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                    queue_flush(routing_table);
                    return EXIT_FAILURE;
                }

                break;

            case STATE_UPDATE:

                /* deliberately sending update, specifying triggering host to be a */
                /* non-existing host. */
                wc = send_update(sockfd, routing_table, 255, mip_address);
                if (wc == -1)
                {
                    close(sockfd); close(timerfd); close(epollfd);
                    free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                    queue_flush(routing_table);
                    return EXIT_FAILURE;
                }

                state = STATE_WAIT;
                break;

            case STATE_EXIT:
                close(sockfd); close(timerfd); close(epollfd);
                free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                queue_flush(routing_table);
                return EXIT_FAILURE;

            default:
                fprintf(stderr, "<routing>: undefined state, exiting...\n");
                close(sockfd); close(timerfd); close(epollfd);
                free(sdu->payload); free(sdu); free_routing_table(routing_table); 
                queue_flush(routing_table);
                return EXIT_FAILURE;
        }      
    } while (1);

    /* for valgrind debugging */
    printf("<routing>: user interruption\n");
    close(sockfd); close(timerfd); close(epollfd);
    free(sdu->payload); free_routing_table(routing_table); free(sdu); queue_flush(routing_table);
    return EXIT_SUCCESS;
}

struct mip_routing_table_entry *check_for_timeouts(queue *routing_table, uint8_t hello_count)
{
    int i;
    struct queue_entry *qe = routing_table->head;
    struct mip_routing_table_entry *re;
    int table_len = (int) queue_length(routing_table);

    for (i = 0; i < table_len; i++)
    {
        re = (struct mip_routing_table_entry*) qe->data;
        
        /* is adjacent has is not in sync with hello counter */
        if (re->hops == 1 && re->hello_count != hello_count)
        {
            return re;
        }
        
        qe = qe->next;
    }

    return NULL;
}

int update_entry(queue *routing_table, uint8_t src, uint8_t dest, uint8_t hops, uint8_t hello_count)
{
    int i, existing = 0;
    struct queue_entry *qe = routing_table->head;
    struct mip_routing_table_entry *re = NULL, *new_e;
    int table_len = (int) queue_length(routing_table);

    /* All paths implementation */
    new_e = allocate_memory(sizeof(struct mip_routing_table_entry));
    if (new_e == NULL)
    {
        return -1;
    }

    new_e->dest             = dest;
    new_e->hops             = hops;
    new_e->next_hop         = src;
    new_e->hello_count      = hello_count;

    /* edge case if we add first entry to the table */
    if (qe == NULL)
    {
        queue_tail_push(routing_table, new_e);
        return 1;
    }

    /* normal use case */
    else 
    {
        /* only adding if we have not stored the entry */
        for (i = 0; i < table_len; i++)
        {
            re = (struct mip_routing_table_entry*) qe->data;

            /* we already have the stored entry */
            if (re->dest == dest && re->next_hop == src && re->hops == hops)
            {
                existing = 1;
            }

            else if (re->dest == dest && re->next_hop == src && re->hops == INFINITY)
            {
                re->hops = hops;
                existing = 1;
            }

            /* destination and next hops is already stored, but path is proposed to be unreachable. */
            else if (re->dest == dest && re->next_hop == src && hops == INFINITY)
            {
                re->hops = hops;
            }

            qe = qe->next;
        }

        if (!existing)
        {
            queue_tail_push(routing_table, new_e);
            return 1;
        }

        free(new_e);

        return 0;
    }
}

int update_table(queue * routing_table, struct mip_sdu *sdu, uint8_t mip_address, uint8_t hello_count)
{
    int i, k;
    uint8_t src, dest, hops;
    int updated = 0; 
    int update_size = sdu->payload[4];

    for (i = 0; i < update_size; i++)
    {
        /* if this host is the next in the path, we skip the update. This is in order */
        /* to prevent overwriting the path for to a broken link */
        if (sdu->payload[i * 3 + 6] == mip_address) continue;

        /* else, we send the entry to update function */
        src     = sdu->payload[3];
        dest    = sdu->payload[i * 3 + 5];
        hops    = ((uint8_t) sdu->payload[i * 3 + 7] == INFINITY) 
            ? sdu->payload[i * 3 + 7] : sdu->payload[i * 3 + 7] + 1;

        k = update_entry(routing_table, src, dest, hops, hello_count);
        if (k)
        {
            updated = 1;
            if (DEBUG) 
            {
                printf("<routing>: updated routing table\n");
                mip_print_routing_table(routing_table);
            }

        } 
    }

    return updated;
}

int recv_from_daemon(int socket, mip_sdu *sdu)
{
    int rc;
    char buf[MAX_RT_PKT_SIZE]; /* max size of routing table */
    rc = read(socket, buf, MAX_RT_PKT_SIZE);
    if (rc <= 0)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("read");
        return -1;
    }

    mip_deserialize_sdu(buf, sdu, rc);
    return 0;
}

int send_hello(int socket, uint8_t src)
{
    int wc;
    char buf[HEL_SIZE] = {0};

    buf[0] = src;
    buf[1] = 0;
    buf[2] = 'H';
    buf[3] = 'E';
    buf[4] = 'L';
    buf[5] = src;

    wc = write(socket, buf, HEL_SIZE);
    if (wc == -1)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("write");  
    }
    return wc;
}

int send_update(int socket, struct queue *routing_table, uint8_t last_upd_src, uint8_t src)
{

    int wc, i, j, added = 0;
    struct queue_entry *qe = routing_table->head;
    struct mip_routing_table_entry *e;
    int table_len = (int) queue_length(routing_table);
    int bufsize = UPD_SIZE + 3 * table_len;
    int upd_size = UPD_SIZE;
    char buf[bufsize];
    memset(buf, 0, bufsize);

    /* buf[0] will be target host */
    buf[1] = 0;
    buf[2] = 'U';
    buf[3] = 'P';
    buf[4] = 'D';
    buf[5] = src;

    /* only add shortest paths */ 
    for (i = 0; i < table_len; i++)
    {
        e = (struct mip_routing_table_entry*) qe->data;

        /* checking if entry is already added to the buffer */
        for (j = UPD_SIZE; j < bufsize; j += 3)
        {
            /* if it's already in buffer */
            if (buf[j] == e->dest)
            {
                added = 1;
                break;
            }
        }

        /* if it already stored in buffer, look for better paths */
        /* j will be index of where it is stored */
        if (added)
        {
            /* this entry is "better" */
            if (e->hops < buf[j + 2])
            {
                buf[j] = e->dest;
                buf[j + 1] = e->next_hop;
                buf[j + 2] = e->hops;
            }

            i--;
        }

        /* if not, store it in buffer */
        else 
        {
            buf[UPD_SIZE + 3 * i] = e->dest;
            buf[UPD_SIZE + 3 * i + 1] = e->next_hop;
            buf[UPD_SIZE + 3 * i + 2] = e->hops;
            upd_size += 3;
        }

        qe = qe->next;
        if (qe == NULL) break;
        added = 0;
    }

    buf[6] = (upd_size - UPD_SIZE) / 3;

    qe = routing_table->head;
    /* write update for each neighbour */
    for (i = 0; i < table_len; i++)
    {
        e = (struct mip_routing_table_entry*) qe->data;

        /* if node is neighbour and not were update came from */
        if (e->hops == 1 && e->next_hop != last_upd_src)
        {
            buf[0] = e->next_hop;

            /* loop through all next_hop fields and set poison reverse */
            for (j = 0; j < UPD_SIZE + 1; j++)
            {
                if (buf[j] == buf[0]) buf[j + 1] = INFINITY;
            }

            wc = write(socket, buf, upd_size);
            if (wc == -1)
            {
                fprintf(stderr, "%s() ", __FUNCTION__);
                perror("write");
                return -1;
            }
        }
        qe = qe->next;
    }

    return 0;
}

static int lookup(queue *routing_table, struct mip_routing_table_entry *e, uint8_t req)
{
    int i;
    struct queue_entry *qe = routing_table->head;
    struct mip_routing_table_entry *re;
    e->hops = INFINITY;
    for (i = 0; i < (int) queue_length(routing_table); i++)
    {
        re = (struct mip_routing_table_entry*) qe->data;
        if (re->dest == req && re->hops != INFINITY)
        {
            /* get better path */
            if (re->hops < e->hops) *e = *re;
        } 
        qe = qe->next;
    }

    if (e->hops != INFINITY) return 0;
    if (DEBUG) 
    {
        printf("<routing>: did not find matching MIP address\n");
    }
    e->next_hop  = MAX_MIP_ADDR;
    return -1;
}

int send_routing_res(int socket, queue *routing_table, uint8_t src, uint8_t req)
{
    int wc;
    struct mip_routing_table_entry e;
    char buf[RES_SIZE] = {0};
    
    wc = lookup(routing_table, &e, req);

    buf[0] = src;
    buf[1] = e.hops;
    buf[2] = 'R';
    buf[3] = 'E';
    buf[4] = 'S';
    buf[5] = e.next_hop;

    wc = write(socket, buf, RES_SIZE);
    if (wc == -1)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("write");
        return -1;
    }

    return 0;
}

void update_hello_count(queue *routing_table, uint8_t src, uint8_t hello_count)
{
    int i;
    struct queue_entry *qe = routing_table->head;
    struct mip_routing_table_entry *re;
    int table_len = (int) queue_length(routing_table);

    for (i = 0; i < table_len; i++)
    {
        re = (struct mip_routing_table_entry*) qe->data;
        
        if (re->hops == 1 && re->dest == src) /* is adjacent host */
        {
            re->hello_count = hello_count;
            return;
        }
        
        qe = qe->next;
    }
}

void free_routing_table(queue *routing_table)
{
    int i;
    int len = (int) queue_length(routing_table);
    struct queue_entry *qe = routing_table->head;
    struct mip_routing_table_entry *e;

    for (i = 0; i < len; i++)
    {
        e = (struct mip_routing_table_entry*) qe->data;
        free(e);
        
        qe = qe->next;
    }
}
