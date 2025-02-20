#include "function.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

using namespace std;

int build_server(const char* port) {
    struct addrinfo hints{}, *host_info_list;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const char* port_to_use = (port[0] == '\0') ? "0" : port;

    int status = getaddrinfo(NULL, port_to_use, &hints, &host_info_list);
    if (status != 0) {
        cerr << "Error: cannot get address info for host: " 
             << gai_strerror(status) << endl;
        return -1;
    }

    int socket_fd = -1;
    struct addrinfo* p;
    for(p = host_info_list; p != NULL; p = p->ai_next) {
        socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        int yes = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            close(socket_fd);
            continue;
        }

        status = ::bind(socket_fd, p->ai_addr, p->ai_addrlen);
        if (status == -1) {
            close(socket_fd);
            continue;
        }

        break;
    }

    freeaddrinfo(host_info_list);

    if (p == NULL) {
        cerr << "Error: failed to bind to port " << port_to_use << endl;
        return -1;
    }

    if (listen(socket_fd, 100) == -1) {
        cerr << "Error: cannot listen on socket" << endl;
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

int build_client(const char* hostname, const char* port) {
    if (!hostname || !port || strlen(hostname) == 0 || strlen(port) == 0) {
        cerr << "Error: hostname and port cannot be empty" << endl;
        return -1;
    }

    struct addrinfo hints{}, *host_info_list;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, port, &hints, &host_info_list);
    if (status != 0) {
        cerr << "Error: cannot get address info for host (" << hostname << ", " << port << ")" << endl;
        return -1;
    }

    int socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (socket_fd == -1) {
        cerr << "Error: cannot create socket (" << hostname << ", " << port << ")" << endl;
        freeaddrinfo(host_info_list);
        return -1;
    }

    status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        cerr << "Error: cannot connect to socket (" << hostname << ", " << port << ")" << endl;
        freeaddrinfo(host_info_list);
        close(socket_fd);
        return -1;
    }

    freeaddrinfo(host_info_list);
    return socket_fd;
}

int server_accept(int socket_fd, std::string* ip) {
    struct sockaddr_storage socket_addr;
    socklen_t addr_len = sizeof(socket_addr);
    int client_fd = accept(socket_fd, reinterpret_cast<struct sockaddr*>(&socket_addr), &addr_len);
    if (client_fd == -1) {
        cerr << "Error: cannot accept connection on socket" << endl;
        return -1;
    }
    
    struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(&socket_addr);
    *ip = inet_ntoa(addr_in->sin_addr);
    return client_fd;
}

int get_port_num(int socket_fd) {
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(socket_fd, reinterpret_cast<struct sockaddr*>(&sin), &len) == -1) {
        cerr << "Error: cannot getsockname" << endl;
        return -1;
    }
    return ntohs(sin.sin_port);
}
