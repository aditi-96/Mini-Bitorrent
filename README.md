# Build Commands
- `client`: g++ client.cpp -pthread -lcrypto -o client
- `server`: g++ -pthread server.cpp -o server

# Execution
- `client`: ./client <client_ip> <client_port> <server_ip> <server_port>
	- eg: `./client 127.0.0.2 9000 127.0.0.1 49152`
- `server`: ./server <server_ip> <server_port>
	- eg: `./server 127.0.0.1 49152`

