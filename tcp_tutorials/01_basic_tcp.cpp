#include <iostream>      // std::cout, std::cerr for printing to the terminal
#include <cstring>       // std::memset for zero-initializing structs/buffers
#include <sys/socket.h>  // socket(), bind(), listen(), accept(), recv(), send()
#include <netinet/in.h>  // sockaddr_in, htons(), INADDR_ANY
#include <arpa/inet.h>   // inet_ntoa() to print IPv4 addresses (optional)
#include <unistd.h>      // close() to close file descriptors

using namespace std;
const int PORT = 5003;

int main() {
    // 1) CREATE A SOCKET
    //
    // Type: int
    // - What it is: a file descriptor (FD --> basically an integer ID that OS uses to identify an I/O resource) 
    //   that represents the listening socket in the OS.
    // - How it is used: we pass it to bind(), listen(), accept(), and eventually close().
    int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        cerr << "socket() failed" << endl;
        return 1;
    }

    // 2) PREPARE A SOCKADDR_IN STRUCT FOR THE SERVER ADDRESS
    //
    // Type: struct sockaddr_in
    // - What it is: IPv4-specific socket address structure.
    // - Concept (high level): "Where on this machine should my server live?" → IP + port.
    // - Implementation detail: a C struct with several fields (family, port, raw IP bits).
    sockaddr_in server_addr{
        .sin_family = AF_INET, 
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(static_cast<uint16_t>(PORT))
    }; 

    // 3) BIND THE SOCKET TO THE ADDRESS
    //
    // ::bind:
    // - Type: function returning int
    // - What it is: asks the OS to associate (server_fd, server_addr) with the local machine.
    // - How it is used: after bind(), the socket has a fixed local IP/port.
    if (::bind(server_fd,
               reinterpret_cast<sockaddr*>(&server_addr),
               sizeof(server_addr)) == -1) {
        perror("bind");
        ::close(server_fd);
        return 1;
    }

    // 4) PUT THE SOCKET INTO LISTENING MODE
    //
    // ::listen:
    // - Type: function returning int
    // - What it is: marks the socket as passive, ready to accept incoming connections.
    // - How it is used: the second argument is the backlog (max pending connections).
    if (::listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        ::close(server_fd);
        return 1;
    }

    cout << "Basic TCP server listening on port " << PORT << endl;

    // 5) ACCEPT A SINGLE CLIENT CONNECTION
    //
    // client_addr:
    // - Type: struct sockaddr_in
    // - What it is: will hold the client's address information (IP + port).
    // - How it is used: accept() fills this in with the connecting client's details.
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr); // Type: socklen_t (length in bytes)

    // ::accept:
    // - Type: function returning int (new socket FD)
    // - What it is: blocks until a client connects, then returns a new FD for that connection.
    // - How it is used: we use client_fd to talk to this specific client.
    int client_fd = ::accept(server_fd,
                             reinterpret_cast<sockaddr*>(&client_addr),
                             &client_addr_len);

    if (client_fd == -1) {
        perror("accept");
        ::close(server_fd);
        return 1;
    }

    // Optional: print the client's IP and port so we can see who connected.
    cout << "Client connected from "
         << ::inet_ntoa(client_addr.sin_addr)  // converts IPv4 address to dotted string
         << ":" << ntohs(client_addr.sin_port) // converts port from network to host order
         << endl;

    // 6) RECEIVE SOME DATA FROM THE CLIENT
    //
    // buffer:
    // - Type: fixed-size C array of char
    // - What it is: temporary storage for bytes we read from the client.
    // - How it is used: recv() writes into buffer, up to its size.
    char buffer[1024];

    // bytes_received:
    // - Type: ssize_t (signed size type)
    // - What it is: number of bytes actually read from the client, or -1 on error, or 0 if EOF.
    // - How it is used: determines how many bytes to process / send back.
    ssize_t bytes_received = ::recv(client_fd, buffer, sizeof(buffer), 0);

    if (bytes_received < 0) {
        perror("recv");
        ::close(client_fd);
        ::close(server_fd);
        return 1;
    }

    if (bytes_received == 0) {
        cout << "Client closed the connection without sending data." << endl;
        ::close(client_fd);
        ::close(server_fd);
        return 0;
    }

    // 7) SEND THE SAME DATA BACK (ECHO)
    //
    // ::send:
    // - Type: function returning ssize_t
    // - What it is: writes bytes from our buffer to the client socket.
    // - How it is used: we send exactly the bytes we received to implement a simple echo.
    ssize_t bytes_sent = ::send(client_fd, buffer, static_cast<size_t>(bytes_received), 0);

    if (bytes_sent < 0) {
        perror("send");
    } else {
        cout << "Echoed " << bytes_sent << " bytes back to the client." << endl;
    }

    // 8) CLOSE BOTH SOCKETS
    //
    // ::close:
    // - Type: function returning int
    // - What it is: tells the OS we are done with this file descriptor.
    // - How it is used: releases the underlying network resources.
    ::close(client_fd);
    ::close(server_fd);

    cout << "Server shut down cleanly." << endl;
    return 0;
}


