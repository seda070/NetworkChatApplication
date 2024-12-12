# Network Chat Application
A simple chat application built in C for real-time communication over a network.
## Features
- Supports multiple clients.
- Real-time message broadcasting.
- Server-client architecture.
- Written in C for performance and simplicity.
## Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/seda070/NetworkChatApplication.git
2.Navigate to directory
```bash
cd NetworkChatApplication
3.Compile the projects
```bash
gcc server.c clientList.c -o server
```bash
gcc client.c -o client
4.Starting server
```bash
Usage: ./server <PORT> (--optional - default 8080)
5.Starting client
```bash
Usage: ./client <IP_ADDRESS> <PORT>



