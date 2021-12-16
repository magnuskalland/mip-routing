#include "../headers/ping_server.h"
#include "../headers/mip.h"
#include "../headers/utils.h"
#include "../headers/common.h"
#include "../headers/mip_debug.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h> 


int main(int argc, char* argv[]) 
{
    int                 c, rc;
    int                 sockfd, epollfd;
    char                *socket_lower, entity_type = MIP_PING + '0';
    struct epoll_event  events_struct = {0}, events[MAX_EVENTS] = {0};

    if (argc < 2 || argc > 3)
    {
        printf("%s\n", "usage: ./ping_server [-h] <socket_lower>");
        return EXIT_SUCCESS;
    }

    while ((c = getopt(argc, argv, "h")) != -1)
    {
        if (c == 'h')
        {
            printf("%s\n", "-h >> usage: ./ping_server [-h] <socket_lower>");
            return EXIT_SUCCESS;
        }
    }

    socket_lower = argv[1];
    sockfd = mip_connect_unix_socket(socket_lower, entity_type);
    if (sockfd == -1)
    {
        fprintf(stderr, " >>> <server>: did you remember to start the daemon?\n");
        return EXIT_FAILURE;
    }

    epollfd = epoll_setup(&events_struct, sockfd);
    if (epollfd == -1) 
    {
        perror("epoll_create1");
        unlink(socket_lower);
        return EXIT_FAILURE;
    }

    while (1)
    {
        rc = epoll_wait(epollfd, events, MAX_EVENTS, -1);

        /* error */
        if (rc == -1)
        {
            perror("epoll_wait");
            unlink(socket_lower);
            close(sockfd); close(epollfd);
            return EXIT_FAILURE;
        } 

        /* handle incoming packet */
        else if (events -> data.fd == sockfd)
        {
            if (handle_client(sockfd) == -1)
            {
                unlink(socket_lower);
                close(sockfd); close(epollfd);
                return EXIT_FAILURE;
            }
        }
    }
}

static int handle_client(int socket) 
{
    char *buf;
    int rc, wc;
    mip_sdu sdu;

    buf = allocate_memory(MAX_MSG_SIZE);
    if (buf == NULL)
        return -1;

    rc = read(socket, buf, MAX_MSG_SIZE);
    if (rc == -1)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("read");
        free(buf);
        return -1;
    }

    else if (rc == 0)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("read");
        free(buf);
        return -1;
    }

    sdu.payload = allocate_memory(rc);
    if (sdu.payload == NULL)
    {
        free(buf);
        return -1;
    }

    mip_deserialize_sdu(buf, &sdu, rc);

    printf("<server>: %s\n", sdu.payload);
    free(buf);

    /* getting the standard server response */
    buf = get_msg(PONG, &sdu.payload[6]);
    if (buf == NULL)
        return -1;

    /* we don't need the ping message anymore */
    free(sdu.payload);
    sdu.payload = allocate_memory(strlen(buf));
    memcpy(sdu.payload, buf, strlen(buf));
    sdu.ttl = DEFAULT_TTL;

    mip_serialize_sdu(buf, &sdu);

    wc = write(socket, buf, strlen(sdu.payload) + 2);
    if (wc == -1)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("write");
        free(buf);
        return -1;
    }
    
    free(buf);
    return 0;
}