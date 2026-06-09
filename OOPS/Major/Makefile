# Makefile for Multi-Client Chat System

# Use g++
CXX = g++
# Look for header files in the include directory
# Upgraded to C++17 for better support of modern networking libraries
CXXFLAGS = -Wall -std=c++17 -pthread -I./include

# Targets
all: discovery_srv fork_srv thread_srv select_srv client_bin

# Dependency on protocol.h added to all rules
discovery_srv: servers/discovery_server.cpp include/protocol.h
	$(CXX) $(CXXFLAGS) servers/discovery_server.cpp -o discovery_srv

fork_srv: servers/fork_server.cpp include/protocol.h
	$(CXX) $(CXXFLAGS) servers/fork_server.cpp -o fork_srv

thread_srv: servers/thread_server.cpp include/protocol.h
	$(CXX) $(CXXFLAGS) servers/thread_server.cpp -o thread_srv

select_srv: servers/select_server.cpp include/protocol.h
	$(CXX) $(CXXFLAGS) servers/select_server.cpp -o select_srv

client_bin: client/chat_client.cpp include/protocol.h
	$(CXX) $(CXXFLAGS) client/chat_client.cpp -o client_bin

# Clean up binaries and metrics logs
clean:
	rm -f discovery_srv fork_srv thread_srv select_srv client_bin *_metrics.csv