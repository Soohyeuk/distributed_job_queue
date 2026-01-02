#include <arpa/inet.h>  // inet_ntoa() to print IPv4 addresses (optional)
#include <cstring>      // std::memset for zero-initializing structs/buffers
#include <iostream>     // std::cout, std::cerr for printing to the terminal
#include <netinet/in.h> // sockaddr_in, htons(), INADDR_ANY
#include <signal.h>
#include <sys/socket.h> // socket(), bind(), listen(), accept(), recv(), send()
#include <unistd.h>     // close() to close file descriptors

using namespace std;
const int PORT = 5003;

// accepts multple message per client; handled by a child process
void handle_client(int client_fd) {
  while (true) {
    char data[1024];
    ssize_t message = ::recv(client_fd, data, sizeof(data), 0);

    if (message > 0) {
      string input(data, message);

      if (input == "QUIT\n") {
        cout << "QUIT command registered." << endl;
        ::close(client_fd);
        break;
      }

      // echo back
      int sending = ::send(client_fd, data, static_cast<size_t>(message), 0);
      if (sending < 0) {
        cerr << "Sending message failed" << endl;
        ::close(client_fd);
      } else {
        cout.write(data, message);
      }

    } else if (message == 0) {
      cout << "Terminated" << endl;
      ::close(client_fd);
      break;
    } else {
      cerr << "Error receiving message" << endl;
      ::close(client_fd);
      break;
    }
  }
};

int main() {
  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    cerr << "Socket creation failed" << endl;
    return 0;
  };

  int opt = 1;
  if (::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    cerr << "Resuing a port had an issue" << endl;
    ::close(server_fd);
    return 1;
  }

  sockaddr_in server_struct{.sin_family = AF_INET,
                            .sin_addr.s_addr = htonl(INADDR_ANY),
                            .sin_port = htons(static_cast<u_int16_t>(PORT))};

  int binding = ::bind(server_fd, reinterpret_cast<sockaddr *>(&server_struct),
                       sizeof(server_struct));
  if (binding == -1) {
    cerr << "Binding failed" << endl;
    ::close(server_fd);
    return 0;
  };

  int listening = ::listen(server_fd, SOMAXCONN);
  if (listening == -1) {
    cerr << "Server listening failed" << endl;
    ::close(server_fd);
    return 0;
  };

  cout << "TCP Concurrent Server started in localhost " << PORT << endl;

  sockaddr_in client_struct;
  signal(SIGCHLD, SIG_IGN);

  while (true) {
    // accepting a new client
    socklen_t len_client = sizeof(client_struct);
    int client_fd = ::accept(
        server_fd, reinterpret_cast<sockaddr *>(&client_struct), &len_client);

    if (client_fd == -1) {
      cerr << "Client creation failed" << endl;
      ::close(client_fd);
      continue;
    }

    // to achieve concurrent client messaging, we fork
    pid_t pid = fork();
    if (pid < 0) {
      cerr << "Forking failed" << endl;
      ::close(client_fd);

    } else if (pid == 0) { // child process
      cout << "child" << endl;
      ::close(server_fd); // because child forks the fd, we want to close this
      handle_client(client_fd);
      return 0;

    } else {
      cout << "parent" << endl;
      ::close(client_fd);
    }
  }

  return 0;
}