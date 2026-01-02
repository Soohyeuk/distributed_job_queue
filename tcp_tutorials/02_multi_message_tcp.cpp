#include <arpa/inet.h>  // inet_ntoa() to print IPv4 addresses (optional)
#include <cstring>      // std::memset for zero-initializing structs/buffers
#include <iostream>     // std::cout, std::cerr for printing to the terminal
#include <netinet/in.h> // sockaddr_in, htons(), INADDR_ANY
#include <sys/socket.h> // socket(), bind(), listen(), accept(), recv(), send()
#include <unistd.h>     // close() to close file descriptors

using namespace std;
const int PORT = 5003;

int main() {
  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);

  if (server_fd == -1) {
    cerr << "Creating socket failed, try again." << endl;
    ::close(server_fd);
    return 1;
  }

  sockaddr_in server_struct{.sin_family = AF_INET,
                            .sin_addr.s_addr = htonl(INADDR_ANY),
                            .sin_port = htons(static_cast<uint16_t>(PORT))};
  sockaddr_in client_struct;

  int opt = 1;
  if (::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    cerr << "Resuing a port had an issue" << endl;
    ::close(server_fd);
    return 1;
  }

  // since i defined a socket fd and struct to use for socket func
  // i should now create a listen and accept things
  // no i bind first, so the OS knows what socket is what
  int binding = ::bind(server_fd, reinterpret_cast<sockaddr *>(&server_struct),
                       sizeof(server_struct));
  if (binding == -1) {
    cerr << "Binding error" << endl;
    ::close(server_fd);
    return 1;
  };

  // now we listen:
  int server_listen = ::listen(server_fd, SOMAXCONN);
  if (server_listen == -1) {
    cerr << "Server listening error" << endl;
    ::close(server_fd);
    return 1;
  };

  cout << "Basic TCP server listening on port " << PORT << endl;

  socklen_t client_len = sizeof(client_struct);
  int client_fd = ::accept(
      server_fd, reinterpret_cast<sockaddr *>(&client_struct), &client_len);

  if (client_fd == -1) {
    cerr << "Failed to accept connection" << endl;
    ::close(server_fd);
    return 1;
  }

  char buffer[1024];
  while (true) {
    ssize_t message = ::recv(client_fd, buffer, sizeof(buffer), 0);

    if (message > 0) {
      string input(buffer, message);

      if (input == "QUIT\n") {
        cout << "Thank you for quitting :)" << endl;
        ::close(server_fd);
        ::close(client_fd);
        return 1;
      }

      // send back to echo
      ssize_t message_sent =
          ::send(client_fd, buffer, static_cast<size_t>(message), 0);

      if (message_sent < 0) {
        cerr << "Send did not work" << endl;
      } else {
        cout.write(buffer, message);
      }

    } else if (message < 0) {
      // error occurred
      cerr << "Error occurred while ingesting data" << endl;
      ::close(server_fd);
      ::close(client_fd);
      return 1;

    } else {
      // closed it
      cout << "Thank you for quitting :)" << endl;
      ::close(server_fd);
      ::close(client_fd);
      return 1;
    }
  };
}