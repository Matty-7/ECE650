#include "function.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cstdlib>

using namespace std;

// Creates a server socket and binds it to the given port
int build_server(const char* port) {
    addrinfo hints{}, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;         // support both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;       // TCP socket
    hints.ai_flags    = AI_PASSIVE;          // auto-fill local address

    const char* use_port = (port[0] == '\0') ? "0" : port;

    int ret = getaddrinfo(nullptr, use_port, &hints, &result);
    if (ret != 0) {
        cerr << "build_server: getaddrinfo failed: " << gai_strerror(ret) << endl;
        exit(EXIT_FAILURE);
    }

    int srv_sock = -1;
    addrinfo* ptr = nullptr;
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        srv_sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (srv_sock < 0) continue;

        int opt = 1;
        if (setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            cerr << "build_server: setsockopt error." << endl;
            close(srv_sock);
            continue;
        }

        if (::bind(srv_sock, ptr->ai_addr, ptr->ai_addrlen) == 0)
            break;  // bound successfully
        
        close(srv_sock);
        srv_sock = -1;
    }
    freeaddrinfo(result);

    if (srv_sock < 0) {
        cerr << "build_server: Unable to bind to port " << use_port << endl;
        exit(EXIT_FAILURE);
    }

    if (listen(srv_sock, 100) < 0) {
        cerr << "build_server: listen failed." << endl;
        close(srv_sock);
        exit(EXIT_FAILURE);
    }
    return srv_sock;
}

// Creates a client TCP connection to the specified hostname and port
int build_client(const char* hostname, const char* port) {
    if (!hostname || !port || strlen(hostname) == 0 || strlen(port) == 0) {
        cerr << "build_client: Invalid hostname or port." << endl;
        exit(EXIT_FAILURE);
    }

    addrinfo hints{}, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(hostname, port, &hints, &result);
    if (ret != 0) {
        cerr << "build_client: getaddrinfo error: " << gai_strerror(ret) << endl;
        exit(EXIT_FAILURE);
    }

    int cli_sock = -1;
    addrinfo* curr = nullptr;
    for (curr = result; curr != nullptr; curr = curr->ai_next) {
        cli_sock = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if (cli_sock < 0) continue;
        if (connect(cli_sock, curr->ai_addr, curr->ai_addrlen) == 0)
            break;  // connection succeeded
        close(cli_sock);
        cli_sock = -1;
    }
    freeaddrinfo(result);

    if (cli_sock < 0) {
        cerr << "build_client: Failed to connect to " << hostname << " on port " << port << endl;
        exit(EXIT_FAILURE);
    }
    return cli_sock;
}

// Accepts an incoming connection and returns the new socket descriptor, also storing the client's IP address
int server_accept(int socket_fd, std::string* ip) {
    sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int new_fd = accept(socket_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_size);
    if (new_fd < 0) {
        cerr << "server_accept: accept error." << endl;
        exit(EXIT_FAILURE);
    }
    
    // Convert client's address to IPv4 string
    sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(&client_addr);
    *ip = inet_ntoa(addr_in->sin_addr);
    return new_fd;
}

// Retrieves the port number to which the given socket is bound
int get_port_num(int socket_fd) {
    sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&local_addr), &len) < 0) {
        cerr << "get_port_num: getsockname error." << endl;
        exit(EXIT_FAILURE);
    }
    return ntohs(local_addr.sin_port);
}
