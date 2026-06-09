#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define SERVER_PORT 8080
#define DISCOVERY_PORT 9000
#define MONITOR_INTERVAL 5
#define METRICS_FILE "server_metrics.csv"

enum MsgType { 
    LOGIN, REGISTER, BROADCAST, PRIVATE, LIST_USERS, EXIT,VIEW_HISTORY, AUTH_SUCCESS, AUTH_FAIL 
};

// __attribute__((packed)) is the secret sauce for your Mac.
// It stops the compiler from adding padding that breaks message reading.
struct __attribute__((packed)) ChatPacket {
    int type;                  
    char sender[32];           
    char recipient[32];        
    char password[32];         
    uint32_t message_len;      
    char content[BUFFER_SIZE]; 
    struct timespec send_time; 
};

#endif