#include <iostream>
#include <map>
#include <string>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <atomic>
#include <fstream>
#include <chrono>
#include <sys/stat.h>
#include <iomanip>
#include "protocol.h"

#define SERVER_ID "THREAD" 

// --- UPDATED LOGGING FUNCTION ---
// This function takes the 'owner' parameter to decide WHICH file to write to.
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

std::atomic<int> active_clients_count(0); 
std::map<std::string, int> active_clients_map;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

const size_t HEADER_SIZE = sizeof(ChatPacket) - BUFFER_SIZE - sizeof(struct timespec);

// Monitoring and PS command helpers...
std::string exec_ps_cmd() {
    char buffer[128];
    std::string result = "";
    std::string cmd = "ps -p " + std::to_string(getpid()) + " -o %cpu,rss | tail -n 1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "0 0";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) result += buffer;
    pclose(pipe);
    return result;
}

void* monitor_thread(void* arg) {
    std::ofstream metrics_log("server_raw.csv", std::ios::app);
    while (true) {
        int current_load = active_clients_count.load();
        if (current_load > 0) {
            std::string stats = exec_ps_cmd();
            double cpu = 0.0; long rss = 0;
            sscanf(stats.c_str(), "%lf %ld", &cpu, &rss);
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            metrics_log << now << "," << cpu << "," << rss << "," << current_load << "\n";
            metrics_log.flush();
        }
        usleep(500000); 
    }
    return NULL;
}

void send_to_client(int sock, ChatPacket& p) {
    send(sock, &p, HEADER_SIZE, 0);
    if (p.message_len > 0) send(sock, p.content, p.message_len, 0);
    send(sock, &p.send_time, sizeof(struct timespec), 0);
}

void* handle_client(void* arg) {
    int client_sock = *(int*)arg;
    delete (int*)arg; 
    ChatPacket packet;
    std::string current_username = "";

    while (true) {
        memset(&packet, 0, sizeof(packet));
        if (recv(client_sock, &packet, HEADER_SIZE, 0) <= 0) break; 
        
        if (packet.message_len > 0 && packet.message_len < BUFFER_SIZE) {
            recv(client_sock, packet.content, packet.message_len, 0);
            packet.content[packet.message_len] = '\0';
        }
        recv(client_sock, &packet.send_time, sizeof(struct timespec), 0);

        if (packet.type == LOGIN) {
            current_username = packet.sender;
            pthread_mutex_lock(&clients_mutex);
            active_clients_map[current_username] = client_sock;
            active_clients_count++;
            pthread_mutex_unlock(&clients_mutex);
            std::cout << "[LOGIN] User: " << current_username << std::endl;
        }
        else if (packet.type == BROADCAST) {
            pthread_mutex_lock(&clients_mutex);
            
            // 1. Log in SENDER'S file
            log_to_file(current_username, "BROADCAST", current_username, "ALL", packet.content);
            
            for (auto const& [name, sock] : active_clients_map) {
                if (sock != client_sock) {
                    send_to_client(sock, packet);
                    // 2. Log in every RECEIVER'S file
                    log_to_file(name, "BROADCAST", current_username, "ALL", packet.content);
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }
        else if (packet.type == PRIVATE) {
            pthread_mutex_lock(&clients_mutex);
            if (active_clients_map.count(packet.recipient)) {
                // 1. Log in SENDER'S file
                log_to_file(current_username, "PRIVATE_SENT", current_username, packet.recipient, packet.content);
                
                // 2. Log in RECIPIENT'S file
                log_to_file(packet.recipient, "PRIVATE_RECV", current_username, packet.recipient, packet.content);
                
                send_to_client(active_clients_map[packet.recipient], packet);
            }
            pthread_mutex_unlock(&clients_mutex);
        }
        else if (packet.type == LIST_USERS) {
            ChatPacket list_pkt;
            memset(&list_pkt, 0, sizeof(list_pkt));
            list_pkt.type = LIST_USERS;
            std::string user_list = "Online Users:\n";
            pthread_mutex_lock(&clients_mutex);
            for (auto const& [name, s] : active_clients_map) user_list += "- " + name + "\n";
            pthread_mutex_unlock(&clients_mutex);
            list_pkt.message_len = user_list.length();
            strncpy(list_pkt.content, user_list.c_str(), BUFFER_SIZE - 1);
            send_to_client(client_sock, list_pkt);
        }
        else if (packet.type == VIEW_HISTORY) {
            std::string path = "conversations_" + std::string(SERVER_ID) + "/" + current_username + ".txt";
            std::ifstream src(path);
            ChatPacket hist;
            memset(&hist, 0, sizeof(hist));
            hist.type = VIEW_HISTORY;
            strcpy(hist.sender, "SERVER");

            if (src) {
                // Read file content. If history is long, it sends the tail to fit BUFFER_SIZE
                std::string body((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
                if (body.length() >= BUFFER_SIZE) {
                    body = body.substr(body.length() - (BUFFER_SIZE - 1));
                }
                strncpy(hist.content, body.c_str(), BUFFER_SIZE - 1);
            } else {
                strcpy(hist.content, "No chat history found.");
            }
            hist.message_len = strlen(hist.content);
            send_to_client(client_sock, hist);
        }
    }

    pthread_mutex_lock(&clients_mutex);
    if (!current_username.empty()) {
        active_clients_map.erase(current_username);
        active_clients_count--;
    }
    close(client_sock);
    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

int main() {
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }
    listen(server_fd, MAX_CLIENTS);

    std::cout << "Thread Server with History enabled on port " << SERVER_PORT << std::endl;

    while (true) {
        int client_socket = accept(server_fd, NULL, NULL);
        if (client_socket < 0) continue;
        int* new_sock = new int(client_socket);
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void*)new_sock);
        pthread_detach(thread_id);
    }
    return 0;
}