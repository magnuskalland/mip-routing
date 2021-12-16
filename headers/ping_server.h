#ifndef PING_SERVER_H
#define PING_SERVER_H


#define PONG        "PONG: "

/**
 * Reads from socket, construct and sends a response back to lower layer.
 * @param client_fd     Lower socket.
 * @return              -1 if error, 0 otherwise.
 * */
static int handle_client(int client_fd);

#endif