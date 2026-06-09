#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sys/select.h>
#include <cstdint> 
#include <fstream>
#include <time.h>
#include <chrono>
#include <pthread.h>
#include <sys/stat.h>
#include <iomanip>
#include <atomic>
#include <array>
#include <memory>
#include <cstdio>
#include "protocol.h"

#define PORT 8080
#define SERVER_ID "SELECT"

// --- STRUCTURED LOGGING FUNCTION ---
void log_to_file(const std::string& owner, const std::string& msg_type, 
                 const std::string& from, const std::string& to, 
                 const std::string& content) {
    if (owner.empty()) return;

    std::string folder = "conversations_" + std::string(SERVER_ID);
    mkdir(folder.c_str(), 0777); 

    std::ofstream file(folder + "/" + owner + ".txt", std::ios::app);
    if (!file.is_open()) return;

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    file << "[" << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "] "
         << "TYPE:" << msg_type << " | "
         << "FROM:" << from << " | "
         << "TO:" << to << " | "
         << "MSG:" << content << std::endl;
}

struct ClientInfo {
    int sock;
    char username[32];
    bool active;
};

const size_t HEADER_SIZE = sizeof(ChatPacket) - BUFFER_SIZE - sizeof(struct timespec);
std::atomic<int> active_clients_count(0);

void send_to_client(int sock, ChatPacket& p) {
    send(sock, &p, HEADER_SIZE, 0);
    if (p.message_len > 0) send(sock, p.content, p.message_len, 0);
    send(sock, &p.send_time, sizeof(struct timespec), 0);
}

// --- MONITORING HELPERS ---
std::string exec_cmd(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "0 0";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) result += buffer.data();
    return result;
}

void* monitor_thread(void* arg) {
    std::ofstream metrics_log("server_raw.csv", std::ios::app);
    while (true) {
        if (active_clients_count > 0) {
            std::string cmd = "ps -p " + std::to_string(getpid()) + " -o %cpu,rss | tail -n 1";
            std::string output = exec_cmd(cmd.c_str());
            double cpu = 0.0; long rss = 0;
            sscanf(output.c_str(), "%lf %ld", &cpu, &rss);
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            metrics_log << now << "," << cpu << "," << rss << "," << active_clients_count << std::endl;
            metrics_log.flush();
        }
        usleep(200000);
    }
    return NULL;
}

int main() {
    ClientInfo clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = false;
        clients[i].sock = 0;
        memset(clients[i].username, 0, 32);
    }

    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);

    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(master_socket, (struct sockaddr *)&address, sizeof(address));
    listen(master_socket, MAX_CLIENTS);

    std::cout << "Select Server (History Enabled) on port " << PORT << "..." << std::endl;

    fd_set readfds;
    
    while (true) {
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        int max_sd = master_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                FD_SET(clients[i].sock, &readfds);
                if (clients[i].sock > max_sd) max_sd = clients[i].sock;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(master_socket, &readfds)) {
            int new_socket = accept(master_socket, NULL, NULL);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i].active) {
                    clients[i].sock = new_socket;
                    clients[i].active = true;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].sock;
            if (clients[i].active && FD_ISSET(sd, &readfds)) {
                ChatPacket packet;
                memset(&packet, 0, sizeof(packet));

                if (recv(sd, &packet, HEADER_SIZE, 0) <= 0) {
                    close(sd);
                    clients[i].active = false;
                    active_clients_count--;
                    memset(clients[i].username, 0, 32);
                    continue;
                }

                if (packet.message_len > 0) recv(sd, packet.content, packet.message_len, 0);
                recv(sd, &packet.send_time, sizeof(struct timespec), 0);

                if (strlen(clients[i].username) == 0) {
                    strncpy(clients[i].username, packet.sender, 31);
                    active_clients_count++;
                }

                if (packet.type == BROADCAST) {
                    // 1. Log for Sender
                    log_to_file(clients[i].username, "BROADCAST", clients[i].username, "ALL", packet.content);
                    
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].active && clients[j].sock != sd) {
                            send_to_client(clients[j].sock, packet);
                            // 2. Log for every recipient
                            log_to_file(clients[j].username, "BROADCAST", clients[i].username, "ALL", packet.content);
                        }
                    }
                }
                else if (packet.type == PRIVATE) {
                    // 1. Log for Sender
                    log_to_file(clients[i].username, "PRIVATE_SENT", clients[i].username, packet.recipient, packet.content);
                    
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].active && strcmp(clients[j].username, packet.recipient) == 0) {
                            send_to_client(clients[j].sock, packet);
                            // 2. Log for recipient
                            log_to_file(clients[j].username, "PRIVATE_RECV", clients[i].username, packet.recipient, packet.content);
                            break;
                        }
                    }
                }
                else if (packet.type == VIEW_HISTORY) {
                    std::string path = "conversations_" + std::string(SERVER_ID) + "/" + std::string(clients[i].username) + ".txt";
                    std::ifstream src(path);
                    ChatPacket hist;
                    memset(&hist, 0, sizeof(hist));
                    hist.type = VIEW_HISTORY;
                    strcpy(hist.sender, "SERVER");

                    if (src) {
                        std::string body((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
                        if (body.length() >= BUFFER_SIZE) body = body.substr(body.length() - (BUFFER_SIZE - 1));
                        strncpy(hist.content, body.c_str(), BUFFER_SIZE - 1);
                    } else {
                        strcpy(hist.content, "No history found.");
                    }
                    hist.message_len = strlen(hist.content);
                    send_to_client(sd, hist);
                }
                else if (packet.type == LIST_USERS) {
                    ChatPacket list_pkt;
                    memset(&list_pkt, 0, sizeof(list_pkt));
                    list_pkt.type = LIST_USERS;
                    strcpy(list_pkt.sender, "Server");
                    std::string list_str = "Online:\n";
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].active) list_str += "- " + std::string(clients[j].username) + "\n";
                    }
                    list_pkt.message_len = list_str.length();
                    strncpy(list_pkt.content, list_str.c_str(), BUFFER_SIZE-1);
                    send_to_client(sd, list_pkt);
                }
            }
        }
    }
    return 0;
}