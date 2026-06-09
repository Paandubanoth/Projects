#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <cstdint>
#include <time.h>
#include "protocol.h"

// Calculate header size: Everything except the content buffer and the timestamp
const size_t HEADER_SIZE = sizeof(ChatPacket) - BUFFER_SIZE - sizeof(struct timespec);

// Helper function to send packets in 3 synchronized parts
void send_packet(int sock, ChatPacket& p) {
    // Fill the timestamp for Task 4 (Benchmarking)
    clock_gettime(CLOCK_REALTIME, &p.send_time);

    // Part 1: Header
    send(sock, &p, HEADER_SIZE, 0);
    // Part 2: Content (if any)
    if (p.message_len > 0) {
        send(sock, p.content, p.message_len, 0);
    }
    // Part 3: Timestamp
    send(sock, &p.send_time, sizeof(struct timespec), 0);
}

void receive_handler(int sock) {
    ChatPacket packet;
    struct timespec recv_time;

    while (true) {
        memset(&packet, 0, sizeof(packet));

        // 1. Read Header
        int n = recv(sock, &packet, HEADER_SIZE, 0);
        if (n <= 0) { 
            // The Server has disconnected or crashed
            std::cout << "\n[ERROR] Connection lost. Server is down." << std::endl;
            close(sock);
            exit(0);  // This kills the WHOLE client immediately
        }

        // 2. Read Content payload
        if (packet.message_len > 0 && packet.message_len < BUFFER_SIZE) {
            int total_rcv = 0;
            while (total_rcv < (int)packet.message_len) {
                int r = recv(sock, packet.content + total_rcv, packet.message_len - total_rcv, 0);
                if (r <= 0) break;
                total_rcv += r;
            }
            packet.content[packet.message_len] = '\0';
        }

        // 3. Read Timestamp
        recv(sock, &packet.send_time, sizeof(struct timespec), 0);
        
        // Task 4: Calculate Latency
        clock_gettime(CLOCK_REALTIME, &recv_time);
        double latency_ms = (recv_time.tv_sec - packet.send_time.tv_sec) * 1000.0 +
                            (recv_time.tv_nsec - packet.send_time.tv_nsec) / 1000000.0;

        // Display message based on type
        if (packet.type == PRIVATE) {
            std::cout << "\r[Private from " << packet.sender << "]: " << packet.content 
                      << " (" << latency_ms << "ms)\n> " << std::flush;
        } else if (packet.type == BROADCAST) {
            std::cout << "\r[Broadcast from " << packet.sender << "]: " << packet.content 
                      << " (" << latency_ms << "ms)\n> " << std::flush;
        } else if (packet.type == LIST_USERS) {
            std::cout << "\r" << packet.content << "\n> " << std::flush;
        }
        if (packet.type == VIEW_HISTORY) {
            std::cout << "\n----------- CHAT HISTORY -----------\n" 
                << packet.content 
                << "------------------------------------\n> " << std::flush;
        } 

    }
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    // --- LOGIN PHASE ---
    ChatPacket login_pkt;
    memset(&login_pkt, 0, sizeof(login_pkt));
    login_pkt.type = LOGIN;
    
    std::cout << "Enter username: ";
    std::cin >> login_pkt.sender;
    std::cout << "Enter password: ";
    std::cin >> login_pkt.password;
    
    send_packet(sock, login_pkt);

    // Start background receiver thread
    std::thread(receive_handler, sock).detach();

    // --- MENU PHASE ---
    int choice;
    while (true) {
        std::cout << "\n1. Broadcast\n2. Private Message\n3. List Users\n4. Veiw History \n5. Quit\n> ";
        if (!(std::cin >> choice)) break;
        std::cin.ignore(); 

        ChatPacket p;
        memset(&p, 0, sizeof(p));
        strncpy(p.sender, login_pkt.sender, 31);

        if (choice == 1) {
            p.type = BROADCAST;
            std::cout << "Message: ";
            std::cin.getline(p.content, BUFFER_SIZE - 1);
            p.message_len = strlen(p.content);
            send_packet(sock, p);
        } else if (choice == 2) {
            p.type = PRIVATE;
            std::cout << "Target User: ";
            std::cin >> p.recipient;
            std::cin.ignore();
            std::cout << "Message: ";
            std::cin.getline(p.content, BUFFER_SIZE - 1);
            p.message_len = strlen(p.content);
            send_packet(sock, p);
        } else if (choice == 3) {
            p.type = LIST_USERS;
            send_packet(sock, p);
        } else if (choice == 5) {
            p.type = EXIT;
            send_packet(sock, p);
            break;
        }
        if (choice == 4) {
            p.type = VIEW_HISTORY;
            send_packet(sock, p);
        }
    }

    close(sock);
    return 0;
}