#include <arpa/inet.h>  // inet_pton
#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket, connect, send, recv
#include <unistd.h>     // close

#include <iostream>
#include <string>

using namespace std;

const int PORT = 5003;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "Usage: " << argv[0] << " <job_name>\n";
    return 1;
  }

  string job = argv[1];

  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    cerr << "Failed to create socket\n";
    return 1;
  }

  sockaddr_in server{
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
  };

  // Convert "127.0.0.1" into binary form
  inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

  // 3. Connect to the server
  if (::connect(sock, (sockaddr *)&server, sizeof(server)) == -1) {
    cerr << "Failed to connect\n";
    close(sock);
    return 1;
  }

  string message = "SUBMIT " + job + "\n";
  if (::send(sock, message.c_str(), message.size(), 0) == -1) {
    cerr << "Failed to send data\n";
  }

  ::close(sock);
  return 0;
}