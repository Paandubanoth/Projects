#include <iostream>
#include <map>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fstream>
#include <chrono>
#include <cstring>
#include "protocol.h" // Ensure this defines ChatPacket, Register, Login, etc.

#define DISCOVERY_PORT 9000
#define METRICS_FILE "discovery_server_metrics.csv"
#define MONITOR_INTERVAL 5 // seconds

struct UserInfo {
    std::string ip;
    int port;
    std::string password;
};

// Map username to their server info
std::map<std::string, UserInfo> discovery_db;
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- TASK 4: Metrics Collection ---
long get_memory_usage() {
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            return std::stol(line.substr(7)); // Returns kB
        }
    }
    return 0;
}
#include <atomic>

// This is thread-safe and process-safe (if in shared memory for Fork)
std::atomic<int> active_clients(0);

// --- TASK 4: Monitoring Thread ---
void* monitor_thread(void* arg) {
    std::ofstream metrics_log(METRICS_FILE);
    metrics_log << "timestamp,memory_kb,registered_users" << std::endl;

    while (true) {
        sleep(MONITOR_INTERVAL);
        
        long mem = get_memory_usage();
        
        pthread_mutex_lock(&db_mutex);
        int users = discovery_db.size();
        pthread_mutex_unlock(&db_mutex);

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        
        metrics_log << now << "," << mem << "," << users << std::endl;
        std::cout << "[MONITOR] Logged: Users=" << users << ", Mem=" << mem << "kB" << std::endl;
    }
    return NULL;
}

// --- Core Discovery Logic ---
void handle_client_request(int client_sock) {
    ChatPacket packet;
    if (recv(client_sock, &packet, sizeof(packet), 0) <= 0) return;

    if (packet.type == REGISTER) {
        // --- Register Chat Server --- [cite: 20]
        UserInfo info;
        // In a real scenario, IP should come from socket, port from packet
        info.port = std::stoi(packet.content); 
        info.password = packet.password;
        
        pthread_mutex_lock(&db_mutex);
        discovery_db[packet.sender] = info;
        pthread_mutex_unlock(&db_mutex);

        std::cout << "[REG] Registered: " << packet.sender << " port: " << info.port << std::endl;
        
        packet.type = AUTH_SUCCESS;
        send(client_sock, &packet, sizeof(packet), 0);
    }
    else if (packet.type == LOGIN) {
        // --- Authenticate Client --- [cite: 23]
        std::cout << "[AUTH] Login attempt: " << packet.sender << std::endl;
        
        pthread_mutex_lock(&db_mutex);
        if (discovery_db.find(packet.sender) != discovery_db.end() && 
            discovery_db[packet.sender].password == packet.password) {
            
            packet.type = AUTH_SUCCESS;
            // Send back the chat server port they should connect to
            std::string port_str = std::to_string(discovery_db[packet.sender].port);
            strcpy(packet.content, port_str.c_str());
        } else {
            packet.type = AUTH_FAIL;
        }
        pthread_mutex_unlock(&db_mutex);
        
        send(client_sock, &packet, sizeof(packet), 0);
    }
}

int main() {
    // --- TASK 4: Start Monitoring Thread ---
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(DISCOVERY_PORT);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    std::cout << "Discovery Server (BENCHMARKING) running on port " << DISCOVERY_PORT << "..." << std::endl;

    while (true) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock < 0) continue;
        
        // Handle request (Register or Login)
        handle_client_request(client_sock);
        close(client_sock);
    }
    return 0;
}