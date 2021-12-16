#include "../headers/mip.h"
#include "../headers/ping_client.h"
#include "../headers/utils.h"
#include "../headers/common.h"
#include "../headers/mip_routing.h"
#include "../headers/mip_daemon.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

int HELP = 0;

int main(int argc, char* argv[]) 
{
    int                 c, wc, rc;
    int                 epollfd, sockfd;
    char                *socket_lower, *message, buf[MAX_MSG_SIZE];
    char                entity_type = MIP_PING + '0';
    uint8_t             dest;
    mip_sdu             mip_sdu;
    struct timeval      start, stop;
    struct epoll_event  events_struct, events[MAX_EVENTS];

    if (argc < 4)
    {
        printf("usage: ./ping_client [-h] <dest_host> <message> <socket_lower>\n");
        return EXIT_SUCCESS;
    }

    while ( ((c = getopt(argc, argv, "h")) != -1))
    {
        switch (c)
        {
            case 'h':
                HELP = 1;
                break;
            default:
                HELP = 0;
                break;
        }
    }

    if (HELP)
    {
        printf("-h >> usage: ./ping_client [-h] <dest_host> <message> <socket_lower>\n");
        return EXIT_SUCCESS;
    }

    dest = atoi(argv[1]);
    message = argv[2] + '\0';
    socket_lower = argv[3];

    /* connect with lower layer */
    sockfd = mip_connect_unix_socket(socket_lower, entity_type);
    if (sockfd == -1)
    {
        fprintf(stderr, " >>> <client>: did you remember to start the daemon?\n");
        return EXIT_FAILURE;
    }

    epollfd = epoll_setup(&events_struct, sockfd);
    if (epollfd == -1) 
    {
        perror("epoll_create1");
        unlink(socket_lower);
        return EXIT_FAILURE;
    }

    mip_sdu.dest        = dest;
    mip_sdu.ttl         = DEFAULT_TTL;
    mip_sdu.payload     = get_msg(PING, message);
    if (mip_sdu.payload == NULL)
    {
        close(sockfd);
        return EXIT_FAILURE;
    }
    mip_serialize_sdu(buf, &mip_sdu);
    gettimeofday(&start, NULL);

    /* Writing to socket */
    wc = write(sockfd, buf, strlen(mip_sdu.payload) + 2);
    if (wc <= 0)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("write");
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* clean buffers */
    memset(buf, 0, MAX_MSG_SIZE);
    free(mip_sdu.payload);

    /* wait for response */

    rc = epoll_wait(epollfd, events, MAX_EVENTS, 1000); /* timeout after 1 second */

    /* error */
    if (rc == -1)
    {
        perror("epoll_wait");
        close(sockfd); close(epollfd);
        return EXIT_FAILURE;
    } 

    if (rc == 0)
    {
        fprintf(stderr, "<client>: timeout, exiting...\n");
        close(sockfd); close(epollfd);
        return EXIT_SUCCESS;
    }

    /* handle incoming packet */
    else if (events -> data.fd == sockfd)
    {
        rc = read(sockfd, buf, MAX_MSG_SIZE);
        if (rc == -1)
        {
            fprintf(stderr, "%s() ", __FUNCTION__);
            perror("read");
            close(sockfd); close(epollfd);
            return EXIT_FAILURE;
        }
        
        else if (rc == 0)
        {
            fprintf(stderr, "<client>: read %d bytes from socket\n", rc);
            close(sockfd); close(epollfd);
            return EXIT_FAILURE;
        }

        gettimeofday(&stop, NULL);

        printf("<client>: finished after %f ms\n",
            (double) (stop.tv_sec - start.tv_sec) * 1000 +
            (double) (stop.tv_usec - start.tv_usec) / 1000000);

        mip_sdu.payload = allocate_memory(rc - 2);
        if (mip_sdu.payload == NULL)
        {
            close(sockfd); close(epollfd);
            return EXIT_FAILURE;
        }
        
        mip_deserialize_sdu(buf, &mip_sdu, rc);
        printf("<client>: %s\n", mip_sdu.payload);
    }

    /* End of process cleanup */
    close(sockfd); close(epollfd);
    free(mip_sdu.payload);
    return EXIT_SUCCESS;
}