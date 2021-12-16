#ifndef COMMON_H
#define COMMON_H

#include <sys/epoll.h>

#define MAX_EVENTS  8
#define NO_CHANGE   -1

/**
 * Reference: 
 * https://github.com/kr1stj0n/plenaries-in3230-h21/blob/main/06-09-2021/unix_sockets/chat.c.  
 * Prepares a socket given from a given socket name, an sets it to listen to incoming connection requests
 * @param socket_name   socket given by user.
 * @return -1 if error, 0 otherwise.
*/
int prepare_unix_socket(char *socket_name);

/**
 * Reference: 
 * https://github.com/kr1stj0n/plenaries-in3230-h21/blob/main/06-09-2021/unix_sockets/chat.c.  
 * Takes a file descriptor an adds it the set of file descriptor in 
 * epoll_struct. Wrapper function for epoll_ctl().
 * @param epoll_fd      the fd associated with the epoll_struct
 * @param epoll_struct  the structure containing the epoll table
 * @param fd_to_add     the file descriptor to add
 * @return -1 if error, 0 otherwise.
*/
int epoll_add_to_table(int epoll_fd, struct epoll_event *epoll_struct, 
    int fd_to_add);

/**
 * Takes a file descriptor and removes it from the epoll table given by 
 * epoll_struct. Wrapper function for epoll_ctl().
 * @param epoll_fd      the fd associated with the epoll_struct
 * @param epoll_struct  the structure containing the epoll table
 * @param fd_to_add     the file descriptor to remove
 * */
int epoll_remove_from_table(int epoll_fd, struct epoll_event *epoll_struct, 
    int fd_to_remove);

/**
 * Called when a connection request has been found on socket_fd. Accepts the 
 * incoming request and adds the new file descriptor to the table given by events_struct
 * @param socket_fd     the socket the incoming request is on
 * @param epoll_fd      the file descriptor associated with the epoll table
 * @param events_struct the structure containing the epoll table
 * @return              -1 if error, new file descriptor if accepted otherwise.
 * */
int handle_connection_request(  int socket_fd, int epoll_fd, 
                                struct epoll_event events_struct);

/**
 * Gets a message formatted the MIP ping way.
 * @param keyword   prefix that will be preprended to the given message
 * @param msg       user specified message
 * */
char* get_msg(char* keyword, char* msg);

int epoll_setup(struct epoll_event *evs, int sockfd);

#endif