###Multi-Architecture Concurrent Network Server Engine###


 ##Project Overview : 
 
This project is a high-performance, concurrent network server engine written in C++ over TCP/IP. It explores, implements, and benchmarks three different operating system 
methodologies for handling multiple client connections simultaneously: Multithreading, Multiprocessing, and I/O Multiplexing.Communication is driven by a custom application-layer
binary protocol (Lightweight Chat Protocol - LCP) engineered with strict message framing rules to ensure reliable, high-throughput distributed messaging across decoupled client nodes.

### How the System Works : 

####1. The Concurrency Engine (Three Swappable Backends)
The core server can be run using three distinct architectural patterns to manage multiple client sockets at the same time:

Multithreaded Model (thread_server.cpp): Spawns a dedicated POSIX thread for every incoming client. While threads share memory naturally, Mutex Locks are strictly enforced to
protect shared states from race conditions during concurrent data access.

Multiprocess Model (fork_server.cpp): Clones a completely isolated child process for each client using the fork() system call. This structure delivers absolute
fault isolation (one client crashing cannot bring down the main server). It handles routing using Unix Pipes and coordinates global state via Shared Memory (mmap).

I/O Multiplexing Model (select_server.cpp): A highly efficient, single-threaded architecture managed by the select() system call. The server monitors an array of active socket descriptors, polling 
and processing data only when a specific socket signals that it is ready for read/write operations.

###2. Custom Binary Protocol & Stream Framing
The LCP Protocol: To eliminate the bandwidth overhead of verbose text formats (like JSON or XML), communication relies on a structured, fixed-size binary packet configuration. 
Packets contain a strict header (Type, Sender, Recipient) and a dynamic payload.

Stream Fragmentation Fix:  Because TCP operates as a continuous byte-stream without message boundaries, the system implements a two-stage read mechanism. The receiver reads
a fixed 4-byte size header first, calculates exactly how many payload bytes are coming, and loops its socket recv() operations until the application packet is perfectly reconstructed in memory.

### 3. State Tracking and Performance Logging
Persistent Session Records: Every active message and transaction is written to client-specific flat-files on the server disk, enabling users to request and stream their past historical data dynamically.

Resource Benchmarking: An automated internal profiling loop captures CPU utilization and Resident Set Size (RSS) memory consumption every 200ms,
writing metrics directly to a CSV file to evaluate architectural scalability under heavy concurrent stress.


## How to Run the Project : 

Follow these steps to compile the codebase and run a multi-client simulation in a distributed terminal setup.

Step 1: Compile the Infrastructure
Run the build automation script in your root project directory to generate all server variants and client binaries simultaneously:

Bash
make clean && make all

Step 2: Launch the Central Registry (Discovery Server)
Open your first terminal window and start the discovery node, which acts as a central phonebook for locating available network services:

Bash
./discovery_serv

Step 3: Spin Up an Active Concurrency Backend
Open a second terminal window and choose one of the three server models to run your infrastructure backend:

Bash
# Option A: Run the Multithreaded Engine
./thread_srv

# Option B: Run the Multiprocess Engine
./fork_srv

# Option C: Run the I/O Multiplexed Engine
./select_srv

Step 4: Connect Multiple Client Nodes
Open two or more separate terminal windows to simulate different concurrent users (e.g., Sanju, Bunny, Paandu) interacting with the system simultaneously:

Bash
./client_bin
