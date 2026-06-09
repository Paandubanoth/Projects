#!/bin/bash
# test_clients.sh - Upgraded for robust client input handling
# Usage: ./test_clients.sh <number_of_clients> <server_type>

NUM_CLIENTS=$1
SERVER_TYPE=$2
PORT=8080

if [ -z "$NUM_CLIENTS" ] || [ -z "$SERVER_TYPE" ]; then
    echo "Usage: ./test_clients.sh <number_of_clients> <fork|thread|select>"
    exit 1
fi

echo "Starting $NUM_CLIENTS clients for $SERVER_TYPE server..."

# Build client if necessary
g++ -std=c++17 client/chat_client.cpp -o client_bin -Iinclude -lpthread

for i in $(seq 1 $NUM_CLIENTS)
do
   # UPGRADE: Use a temporary file for input to ensure robust piping
   # and staggered connection load.
   
   cat <<EOF > input_$i.txt
user$i
/list
/exit
EOF
   
   # Launch client in background using input file
   ./client_bin < input_$i.txt &
   
   # Remove temp file after a short delay
   rm input_$i.txt
   
   # Stagger connections to get better CPU spikes
   sleep 0.1
done

# Wait for clients to finish
wait
echo "Load test complete."