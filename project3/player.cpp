#include "function.h"
#include "potato.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cstring>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: player <master_hostname> <master_port>" << endl;
        return EXIT_FAILURE;
    }
    
    const char* master_host = argv[1];
    const char* master_port = argv[2];

    // connect to ringmaster, get player id and total players
    int ringmaster_fd = build_client(master_host, master_port);
    int curr_player_id = -1, total_players = -1;
    if (recv(ringmaster_fd, &curr_player_id, sizeof(curr_player_id), MSG_WAITALL) <= 0 ||
        recv(ringmaster_fd, &total_players, sizeof(total_players), MSG_WAITALL) <= 0) {
        cerr << "Error: Failed to receive player ID or total player count." << endl;
        close(ringmaster_fd);
        return EXIT_FAILURE;
    }
    
    // create local server socket, for other players to connect (left neighbor)
    int local_server_fd = build_server("");  // empty string means system-assigned port
    int local_port = get_port_num(local_server_fd);
    if (send(ringmaster_fd, &local_port, sizeof(local_port), 0) < 0) {
        cerr << "Error: Cannot send local port to ringmaster." << endl;
        close(ringmaster_fd);
        close(local_server_fd);
        return EXIT_FAILURE;
    }
    
    cout << "Connected as player " << curr_player_id << " out of " << total_players << " total players" << endl;
    
    // receive neighbor's info from ringmaster: neighbor port and IP address
    int neighbor_port = -1;
    char neighbor_ip[100] = {0};
    if (recv(ringmaster_fd, &neighbor_port, sizeof(neighbor_port), MSG_WAITALL) <= 0 ||
        recv(ringmaster_fd, neighbor_ip, sizeof(neighbor_ip), MSG_WAITALL) <= 0) {
        cerr << "Error: Failed to receive neighbor info from ringmaster." << endl;
        close(ringmaster_fd);
        close(local_server_fd);
        return EXIT_FAILURE;
    }
    
    // connect to right neighbor as client
    char neighbor_port_str[10] = {0};
    snprintf(neighbor_port_str, sizeof(neighbor_port_str), "%d", neighbor_port);
    int right_conn_fd = build_client(neighbor_ip, neighbor_port_str);
    
    // accept connection from left neighbor
    string dummy_ip;
    int left_conn_fd = server_accept(local_server_fd, &dummy_ip);
    
    // initialize file descriptors to monitor
    vector<int> fd_list = { right_conn_fd, left_conn_fd, ringmaster_fd };
    int max_fd = *max_element(fd_list.begin(), fd_list.end());
    
    // set random seed (based on current time and player id)
    srand(static_cast<unsigned>(time(nullptr)) + curr_player_id);
    
    Potato hot_potato;

    // main loop: wait for potato message
    while (true) {
        fd_set active_set;
        FD_ZERO(&active_set);
        for (int fd : fd_list) {
            FD_SET(fd, &active_set);
        }
        
        // block until any socket has data
        if (select(max_fd + 1, &active_set, nullptr, nullptr, nullptr) < 0) {
            cerr << "Error: select() failed." << endl;
            break;
        }
        
        int received_bytes = 0;
        for (int fd : fd_list) {
            if (FD_ISSET(fd, &active_set)) {
                received_bytes = recv(fd, &hot_potato, sizeof(Potato), MSG_WAITALL);
                break;
            }
        }
        
        if (received_bytes <= 0 || hot_potato.num_hops == 0) {
            break;
        }
        
        // when remaining hops is 1, it's the last time to pass, need to return to ringmaster
        if (hot_potato.num_hops == 1) {
            hot_potato.num_hops--;
            hot_potato.path[hot_potato.count++] = curr_player_id;
            send(ringmaster_fd, &hot_potato, sizeof(Potato), 0);
            cout << "I'm it" << endl;
        } else {
            // otherwise, continue passing potato: update record and randomly choose left or right neighbor
            hot_potato.num_hops--;
            hot_potato.path[hot_potato.count++] = curr_player_id;
            int choice = rand() % 2; // 0 means left neighbor, 1 means right neighbor
            if (choice == 0) {
                send(left_conn_fd, &hot_potato, sizeof(Potato), 0);
                int left_id = (curr_player_id + total_players - 1) % total_players;
                cout << "Sending potato to " << left_id << endl;
            } else {
                send(right_conn_fd, &hot_potato, sizeof(Potato), 0);
                int right_id = (curr_player_id + 1) % total_players;
                cout << "Sending potato to " << right_id << endl;
            }
        }
    }
    
    close(left_conn_fd);
    close(right_conn_fd);
    close(ringmaster_fd);
    close(local_server_fd);
    
    return EXIT_SUCCESS;
}
