#ifndef FUNCTION_H
#define FUNCTION_H

#include <string>

// build server and bind to the specified port
int build_server(const char *port);

// build client and connect to the specified hostname and port
int build_client(const char *hostname, const char *port);

// accept client connection and return the client's IP address
int server_accept(int socket_fd, std::string *ip);

// get the port number of the specified socket
int get_port_num(int socket_fd);

#endif
