
### 2.
a) Custom Application-Layer Protocol Design
Our custom protocol operates over TCP to ensure reliable, in-order delivery of text messages using a centralized Client-Server architecture.
     It defines a strict communication lifecycle:
--> Connection & Registration:  The client connects to the server and immediately sends a "Login" packet to register its username.
--> Message Exchange:   Clients do not communicate directly. They send structured requests (Broadcast, Private, List) to the server, which acts as a centralized 
    broker to parse and route packets to the correct destination sockets.
--> Termination:    A graceful socket closure or TCP timeout signals the server to remove the user and free network resources.

b). All communication between the client and server is encapsulated in a standardized ChatPacket structure. The message format consists of a fixed 
header (Type, Sender, Recipient) and a data payload (Content).
enum MessageType {
    LOGIN = 0,       // Initial registration packet
    BROADCAST = 1,   // Message to all active users
    PRIVATE = 2,     // Direct message to a specific user
    LIST_USERS = 3   // Request to see active users
};

struct ChatPacket {
    MessageType type;        // The operation being performed (4 bytes)
    char sender[32];         // Username of the sender (32 bytes)
    char recipient[32];      // Target username for PRIVATE messages (32 bytes)
    char content[256];       // The actual message payload (256 bytes)
};
--> Login (type = 0): sender contains the new username. recipient and content are left empty. Tells the server to register the user.
--> Broadcast (type = 1): sender is the user, content contains the text. The server routes this to all connected sockets.
--> Private Message (type = 2): sender is the user, recipient is the target user, content is the text. The server routes this only to the recipient's socket.
--> List Users (type = 3): Client sends this to request a list. The server replies with a packet where sender is "Server" and content contains the newline-separated list of active users.

c). TCP was chosen over UDP because it provides three critical features required for a chat application:
--> Data Integrity: TCP uses checksums and acknowledgments (ACKs) to ensure that text messages are not corrupted or lost during transit.
--> Ordered Delivery: It guarantees that if a user sends "Hello" followed by "How are you?", the recipient receives them in that exact sequence, preventing fragmented or garbled conversations.
--> Flow Control: TCP manages the rate of data transmission between the client and server, preventing the faster sender from overwhelming the slower receiver’s buffer.

d).The receiver (Server or Client) performs a two-stage recv() operation to ensure proper synchronization:
Stage 1)-(Header Read): The receiver calls recv() with a fixed size ( 4 bytes) to read the length of the incoming message.
Stage 2)-(Payload Read): The receiver then calls recv() again, but only for the specific number of bytes defined in the header.
      3). Handling Partial Reads:Because TCP may fragment large messages, the framing logic uses a while loop around the recv() calls. This ensures that the application continues to read
    from the socket buffer until the total number of bytes expected (the "Frame") has been fully reconstructed in memory.

e).
1. System Architecture
We built this system using a Client-Server model over TCP. Since TCP handles the delivery and ordering of data, we can focus on how the server manages users. We implemented three different ways for the server to scale:
Threaded:   Every user gets their own thread. Simple and fast for a moderate number of users.
Fork:       Every user gets a separate process. It’s the most stable because one user crashing won't take down the others.
Select:     A single process handles everyone at once. This is the most memory-efficient way to handle a massive number of connections.

2. The LCP Protocol
To make sure the client and server understand each other perfectly, we created the Lightweight Chat Protocol (LCP). It uses a binary format instead of text to save bandwidth.
Every packet has a fixed structure:
Size:   How big the message is.
Type:   What the message is (Login, Broadcast, Private, or List).
Sender/Recipient:   32-byte slots for names.
Content:        The actual text message.

3. How Messages are Sent (Framing)
--> TCP is a "stream," meaning it doesn't know where one message ends and the next begins. We solved this with Message Framing.
--> The receiver always reads the first 4 bytes to find the Size. Once it knows exactly how many bytes to expect, it pulls that specific amount from the --> socket. This ensures that even if the internet breaks a long message into pieces, the system glues them back together correctly.

4. Build System
--> We use a Makefile to handle the heavy lifting of compilation.
make all:    Compiles everything (the client and all three server versions).
make clean:     Deletes the binaries so you can start from scratch.

To run:
Start a server: ./select_srv
Start clients: ./client
Log in and use the menu to start chatting.



#### 5}c).

1. System Architecture
This project explores three different ways a server can handle multiple clients simultaneously using C++. Each server model manages the same chat logic but handles resources differently.
--> Threaded Server (thread_server.cpp): Spawns a dedicated POSIX thread for every client. This is efficient for memory sharing but can be limited by the system's maximum thread count.
--> Fork Server (fork_server.cpp): Uses fork() to create an independent process for each client. It uses Shared Memory (mmap) for global state and Unix Pipes for inter-process message routing.
--> Select Server (select_server.cpp): A single-process model that uses I/O Multiplexing. It monitors all client sockets at once using the select() system call, making it very lightweight on system resources.

2. Protocol Specification
--> The system communicates using a custom binary protocol to ensure data integrity and to facilitate precise performance tracking. All messages follow a three-part synchronization 

    sequence:
Header:     Fixed-size structure containing message type, sender/recipient names, and payload length.
Payload:    The actual text content (up to 1024 bytes).
Timespec:   A precision timestamp (struct timespec) sent last to calculate end-to-end latency without interference from network buffering. 
Supported Message Types:
LOGIN:      Initial registration.
BROADCAST:  Send to all online users.
PRIVATE:    Targeted messaging between two users.
LIST_USERS:     Request a list of active usernames.
VIEW_HISTORY:       Fetch persistent chat logs from the server's disk.

3. Build System & Execution: 
--> make clean: Removes all compiled binaries and temporary object files to ensure a fresh build.
--> make all: Compiles the client (client) and all three server versions (thread_srv, fork_srv, select_srv) simultaneously.
--> When you run make, the utility looks for a Makefile in the directory. It checks the "Last Modified" timestamp of your source files (ex,thread_server.cpp) against the existing binaries.
If the source is newer than the binary, make triggers the g++ command defined in the script.
It automatically handles linking necessary libraries, such as pthread, so you don't have to type long terminal commands manually.

4. Testing & History
Automated Logging:   Each server creates a folder (e.g., conversations_THREAD/). Inside, you will find a .txt file for every user.
Verification:        Send a message from User A to User B. Open UserB.txt to verify the message was logged with the PRIVATE_RECV tag.
Fetch History:      Use Option 4 in the client. The server reads your specific text file and streams the contents back to your terminal.
--> Benchmarking
Every 200ms, the active server writes its resource usage (CPU/Memory) and current client count to server_raw.csv. You can use this file to track how different architectures scale under load.



### Learnings & Challenges : 
1. Different Server Designs
Threads:     Easy to share data, but a single missing Mutex lock can freeze or crash the whole program.
Fork:        Very stable because each user is separate. The challenge was using Pipes and Shared Memory to let these separate processes talk.
Select:      The most efficient. One process handles everyone using almost no extra RAM.

2. The TCP "Stream" Problem
Learning:     TCP doesn't send "packets" like a post office; it sends a constant stream of data. If you aren't careful, messages get glued together.
Fix:     I used Message Framing. The client sends the "Size" first so the server knows exactly when one message ends and the next begins.

3. Designing the Protocol (LCP)
I learned to build a Binary Protocol. It’s much faster and smaller than using text like JSON.
Sync:   I had to ensure the Header, Content, and Timestamp were always sent in the exact same order, or the data would become "garbage."

4. Tracking Performance
I learned how to pull real-time CPU and RAM stats while the server was under stress.
Insight:     I saw that the Select server barely used more memory when 100 users joined, while the Fork server’s memory usage jumped significantly.

5. Top Challenges
Zombies:            Learning to "reap" dead child processes in the Fork server so they didn't clog the OS.
Synchronization:    Keeping the 3-part send/receive logic perfectly timed so the server and client never got out of step.
Deadlocks:           Hunting down bugs where two threads were waiting on each other, causing the chat to hang.