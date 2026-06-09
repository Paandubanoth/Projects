#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <vector>
#include <sys/stat.h>
#include <iomanip>

#include "protocol.h"

#define SERVER_ID "FORK"

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

struct SharedData {
    char usernames[MAX_CLIENTS][32];
    bool active[MAX_CLIENTS];
    pthread_mutex_t mutex;
};

int comm_pipes[MAX_CLIENTS][2]; 

long get_memory_usage() {
#ifdef __linux__
    std::ifstream file("/proc/self/status");
    std::string line;
    if (!file.is_open()) return 0;
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmRSS:") return std::stol(line.substr(7));
    }
#endif
    return 0; 
}

void* monitor_thread(void* arg) {
    SharedData* shared = static_cast<SharedData*>(arg);
    std::ofstream metrics_log("server_raw.csv", std::ios::app);
    
    while (true) {
        sleep(1);
        long mem = get_memory_usage();
        int clients = 0;
        
        pthread_mutex_lock(&shared->mutex);
        for(int i=0; i<MAX_CLIENTS; i++) if(shared->active[i]) clients++;
        pthread_mutex_unlock(&shared->mutex);

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        metrics_log << now << ",0.0," << mem << "," << clients << std::endl;
        metrics_log.flush();
    }
    return nullptr;
}

void handle_client(int client_sock, int my_slot, SharedData* shared_memory) {
    ChatPacket packet;
    char current_username[32] = "";
    int my_pipe_read = comm_pipes[my_slot][0];

    // Cleanup pipes: Close read ends of others and our own write end
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != my_slot) close(comm_pipes[i][0]);
    }
    close(comm_pipes[my_slot][1]);

    const size_t header_size = sizeof(ChatPacket) - BUFFER_SIZE - sizeof(struct timespec);

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client_sock, &readfds);
        FD_SET(my_pipe_read, &readfds);

        int max_fd = std::max(client_sock, my_pipe_read);
        if (select(max_fd + 1, &readfds, nullptr, nullptr, nullptr) < 0) break;

        // 1. DATA FROM CLIENT
        if (FD_ISSET(client_sock, &readfds)) {
            std::memset(&packet, 0, sizeof(packet));
            if (recv(client_sock, &packet, header_size, 0) <= 0) break;
            if (packet.message_len > 0) recv(client_sock, packet.content, packet.message_len, 0);
            recv(client_sock, &packet.send_time, sizeof(struct timespec), 0);

            if (packet.type == LOGIN) {
                pthread_mutex_lock(&shared_memory->mutex);
                std::strncpy(current_username, packet.sender, 31);
                std::strncpy(shared_memory->usernames[my_slot], current_username, 31);
                shared_memory->active[my_slot] = true;
                pthread_mutex_unlock(&shared_memory->mutex);
                std::cout << "[LOGIN] " << current_username << " on slot " << my_slot << std::endl;
            } 
            else if (packet.type == BROADCAST || packet.type == PRIVATE) {
                pthread_mutex_lock(&shared_memory->mutex);
                
                // Log for the sender
                std::string log_type = (packet.type == BROADCAST) ? "BROADCAST" : "PRIVATE_SENT";
                log_to_file(current_username, log_type, current_username, 
                            (packet.type == BROADCAST ? "ALL" : packet.recipient), packet.content);

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (shared_memory->active[i] && i != my_slot) {
                        bool is_target = (packet.type == BROADCAST) || 
                                         (packet.type == PRIVATE && std::strcmp(shared_memory->usernames[i], packet.recipient) == 0);
                        if (is_target) {
                            // Log for the recipient in their own file
                            std::string rec_log_type = (packet.type == BROADCAST) ? "BROADCAST" : "PRIVATE_RECV";
                            log_to_file(shared_memory->usernames[i], rec_log_type, current_username, 
                                        (packet.type == BROADCAST ? "ALL" : packet.recipient), packet.content);

                            write(comm_pipes[i][1], &packet, header_size);
                            if (packet.message_len > 0) 
                                write(comm_pipes[i][1], packet.content, packet.message_len);
                            write(comm_pipes[i][1], &packet.send_time, sizeof(struct timespec));
                        }
                    }
                }
                pthread_mutex_unlock(&shared_memory->mutex);
            }
            else if (packet.type == VIEW_HISTORY) {
                std::string path = "conversations_" + std::string(SERVER_ID) + "/" + std::string(current_username) + ".txt";
                std::ifstream src(path);
                ChatPacket hist;
                std::memset(&hist, 0, sizeof(hist));
                hist.type = VIEW_HISTORY;
                std::strncpy(hist.sender, "SERVER", 31);

                if (src) {
                    std::string body((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
                    if (body.length() >= BUFFER_SIZE) {
                        body = body.substr(body.length() - (BUFFER_SIZE - 1));
                    }
                    std::strncpy(hist.content, body.c_str(), BUFFER_SIZE - 1);
                } else {
                    std::strcpy(hist.content, "No chat history found.");
                }
                hist.message_len = std::strlen(hist.content);
                
                send(client_sock, &hist, header_size, 0);
                if (hist.message_len > 0) send(client_sock, hist.content, hist.message_len, 0);
                send(client_sock, &hist.send_time, sizeof(struct timespec), 0);
            }
        }

        // 2. DATA FROM OTHER PROCESSES (FORWARD TO CLIENT)
        if (FD_ISSET(my_pipe_read, &readfds)) {
            ChatPacket piped_packet;
            if (read(my_pipe_read, &piped_packet, header_size) > 0) {
                if (piped_packet.message_len > 0) read(my_pipe_read, piped_packet.content, piped_packet.message_len);
                read(my_pipe_read, &piped_packet.send_time, sizeof(struct timespec));
                
                send(client_sock, &piped_packet, header_size, 0);
                if (piped_packet.message_len > 0) send(client_sock, piped_packet.content, piped_packet.message_len, 0);
                send(client_sock, &piped_packet.send_time, sizeof(struct timespec), 0);
            }
        }
    }

    pthread_mutex_lock(&shared_memory->mutex);
    shared_memory->active[my_slot] = false;
    std::memset(shared_memory->usernames[my_slot], 0, 32);
    pthread_mutex_unlock(&shared_memory->mutex);
    
    close(client_sock);
    exit(0);
}

int main() {
    signal(SIGCHLD, SIG_IGN); 

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (pipe(comm_pipes[i]) == -1) {
            perror("Pipe failed");
            return 1;
        }
    }

    SharedData* shared_memory = static_cast<SharedData*>(mmap(nullptr, sizeof(SharedData), 
                                    PROT_READ | PROT_WRITE, 
                                    MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    std::memset(shared_memory, 0, sizeof(SharedData));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_memory->mutex, &attr);

    pthread_t monitor_tid;
    pthread_create(&monitor_tid, nullptr, monitor_thread, shared_memory);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    
    if (::bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_fd, MAX_CLIENTS);
    std::cout << "Fork Server (History Enabled) listening on port " << SERVER_PORT << "..." << std::endl;

    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) continue;

        int slot = -1;
        pthread_mutex_lock(&shared_memory->mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!shared_memory->active[i]) {
                slot = i;
                shared_memory->active[i] = true; 
                break;
            }
        }
        pthread_mutex_unlock(&shared_memory->mutex);

        if (slot != -1) {
            if (fork() == 0) {
                close(server_fd); 
                handle_client(client_socket, slot, shared_memory); 
            } else {
                close(client_socket); 
            }
        } else {
            close(client_socket);
        }
    }
    return 0;
}