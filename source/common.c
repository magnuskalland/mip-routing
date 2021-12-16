#include "../headers/common.h"
#include "../headers/utils.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>

int handle_connection_request(  int socket_fd, int epoll_fd, 
                                struct epoll_event events_struct)
{
    int rc, accept_fd;

    accept_fd = accept(socket_fd, NULL, NULL);
    if (accept_fd == -1)
    {
        perror("accept");
        return -1;
    }

    rc = epoll_add_to_table(epoll_fd, &events_struct, accept_fd);
    if (rc == -1)
        return -1;

    return accept_fd;
}

char* get_msg(char* keyword, char* msg)
{
    char *wrapped_msg;
    wrapped_msg = allocate_memory(strlen(keyword) + strlen(msg));
    if (wrapped_msg == NULL)
        return NULL;

    strcpy(wrapped_msg, keyword);
    strcpy(&wrapped_msg[strlen(keyword)], msg);

    return wrapped_msg;
}

int prepare_unix_socket(char *socket_name) 
{
    int server_fd, true = 1;
    struct sockaddr_un server_addr;

    server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0); /* using SOCK_SEQPACKET to support multiple clients */
    if (server_fd == -1) 
    {
        perror("socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, socket_name);

    unlink(socket_name); /* just for insurance */

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &true, sizeof true) == -1)
    {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) == -1)
    {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }
    
    return server_fd;
}

int epoll_setup(struct epoll_event *evs, int sockfd)
{
    int epollfd, rc;

    epollfd = epoll_create(1); /* argument is ignored in Linux 2.6.8 */
    if (epollfd == -1) 
    {
        perror("epoll_create");
        return -1;
    }

    rc = epoll_add_to_table(epollfd, evs, sockfd);
    if (rc == -1)
    {
        close(epollfd);
        return -1;
    }

    return epollfd;
}

int epoll_add_to_table(int epoll_fd, struct epoll_event *epoll_struct, int fd_to_add)
{
    int rc = 0;
    epoll_struct -> events = EPOLLIN|EPOLLHUP;
    epoll_struct -> data.fd = fd_to_add;

    if ((rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_to_add, epoll_struct)) == -1)
        perror("epoll_ctl");
    return rc;
}

int epoll_remove_from_table(int epoll_fd, struct epoll_event *epoll_struct, int fd_to_remove)
{
    int rc;

    if ((rc = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd_to_remove, epoll_struct)) == -1)
        perror("epoll_ctl");

    return rc;
}