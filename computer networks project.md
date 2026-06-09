# Multi-Architecture Concurrent Network Server Engine

A high-performance C++ communication broker designed over TCP/IP to explore OS-level concurrency and network resource scalability. The system features three completely interchangeable server backends built using distinct system-programming methodologies: Multithreading, Multiprocessing, and Synchronous I/O Multiplexing.

## 🚀 Key Features
* **Three Interchangeable Concurrency Backends:** Compare performance across POSIX Threads, Forked Processes, and `select()` I/O Multiplexing.
* **Lightweight Chat Protocol (LCP):** A custom binary-optimized application-layer protocol ensuring minimal bandwidth overhead compared to text-based payloads like JSON.
* **Deterministic Message Framing:** Solves TCP stream fragmentation ("message bleeding") using a strict two-stage header-payload read loop.
* **Robust IPC:** Implements Unix Pipes for inter-process packet routing and Shared Memory (`mmap`) with process-shared mutexes for state persistence.
* **Performance Benchmarking:** Real-time logging of CPU utilization and Memory (RSS) metrics recorded every 200ms into CSV format.

## 🛠️ Tech Stack
* **Language:** C++ (C++17)
* **OS Environment:** Linux / macOS
* **Networking:** TCP Sockets
* **APIs & Concepts:** POSIX Threads, `fork()`, `select()`, Shared Memory (`mmap`), Unix Pipes

## 📦 How to Build and Run

### 1. Build the system
Compile all servers and the client binary using the automated Makefile:
```bash
make clean && make all
