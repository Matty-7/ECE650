#include "function.h"
#include "potato.h"

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <algorithm>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: ringmaster <port_num> <num_players> <num_hops>" << endl;
        return EXIT_FAILURE;
    }
    
    const char* listen_port = argv[1];
    int total_players = atoi(argv[2]);
    int hop_total = atoi(argv[3]);

    cout << "Potato Ringmaster" << endl;
    cout << "Players = " << total_players << endl;
    cout << "Hops = " << hop_total << endl;

    int master_socket = build_server(listen_port);

    vector<int> player_sockets;
    vector<int> player_listen_ports;
    vector<string> player_ips;

    // accept connection from each player, and pass initial data
    for (int id = 0; id < total_players; ++id) {
        int listen_port_player = 0;
        string client_ip;
        int conn_fd = server_accept(master_socket, &client_ip);
        
        send(conn_fd, &id, sizeof(id), 0);
        send(conn_fd, &total_players, sizeof(total_players), 0);
        
        // receive listening port from player (for connecting to neighbor)
        if (recv(conn_fd, &listen_port_player, sizeof(listen_port_player), MSG_WAITALL) <= 0) {
            cerr << "Error: Failed to receive listening port from player " << id << endl;
            close(conn_fd);
            continue;
        }
        
        player_sockets.push_back(conn_fd);
        player_listen_ports.push_back(listen_port_player);
        player_ips.push_back(client_ip);
        cout << "Player " << id << " is ready to play" << endl;
    }

    // assign right neighbor info to each player
    for (int i = 0; i < total_players; ++i) {
        int neighbor_index = (i + 1) % total_players;
        int neighbor_port = player_listen_ports[neighbor_index];
        const char* neighbor_ip = player_ips[neighbor_index].c_str();
        
        send(player_sockets[i], &neighbor_port, sizeof(neighbor_port), 0);
        char ip_buf[100] = {0};
        strncpy(ip_buf, neighbor_ip, sizeof(ip_buf) - 1);
        send(player_sockets[i], ip_buf, sizeof(ip_buf), 0);
    }

    Potato game_potato;
    game_potato.num_hops = hop_total;

    if (hop_total > 0) {
        // randomly select a player as the starting player
        srand(static_cast<unsigned>(time(NULL)) + total_players);
        int starter = rand() % total_players;
        send(player_sockets[starter], &game_potato, sizeof(game_potato), 0);
        cout << "Ready to start the game, sending potato to player " << starter << endl;

        // wait for potato return
        fd_set active_fds;
        FD_ZERO(&active_fds);
        int max_fd = *max_element(player_sockets.begin(), player_sockets.end());
        for (int sock : player_sockets) {
            FD_SET(sock, &active_fds);
        }
        select(max_fd + 1, &active_fds, nullptr, nullptr, nullptr);
        for (int sock : player_sockets) {
            if (FD_ISSET(sock, &active_fds)) {
                recv(sock, &game_potato, sizeof(game_potato), MSG_WAITALL);
                break;
            }
        }
    }

    // broadcast termination signal: set potato.num_hops to 0
    game_potato.num_hops = 0;
    for (int sock : player_sockets) {
        send(sock, &game_potato, sizeof(game_potato), 0);
        close(sock);
    }

    cout << "Trace of potato:" << endl;
    for (int i = 0; i < game_potato.count; ++i) {
        cout << game_potato.path[i] << (i < game_potato.count - 1 ? "," : "\n");
    }

    close(master_socket);
    return EXIT_SUCCESS;
}
