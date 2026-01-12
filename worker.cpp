#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace std;

const int PORT = 5003;

static bool send_all(int fd, const string &s) {
  const char *p = s.c_str();
  size_t left = s.size();
  while (left > 0) {
    ssize_t n = ::send(fd, p, left, 0);
    if (n <= 0)
      return false;
    p += n;
    left -= static_cast<size_t>(n);
  }
  return true;
}

static bool recv_line(int fd, string &out) {
  out.clear();
  char c;
  while (true) {
    ssize_t n = ::recv(fd, &c, 1, 0);
    if (n <= 0)
      return false;
    if (c == '\n')
      break;
    if (c != '\r')
      out.push_back(c);
  }
  return true;
}

int main() {
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    cerr << "Failed to create socket\n";
    return 1;
  }

  sockaddr_in server{};
  server.sin_family = AF_INET;
  server.sin_port = htons(PORT);
  inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

  if (::connect(sock, (sockaddr *)&server, sizeof(server)) == -1) {
    cerr << "Failed to connect\n";
    close(sock);
    return 1;
  }

  while (true) {
    if (!send_all(sock, "REQUEST\n"))
      break;

    string response;
    if (!recv_line(sock, response))
      break;

    if (response == "EMPTY") {
      this_thread::sleep_for(chrono::milliseconds(300));
      continue;
    }

    cout << "Got job: " << response << endl;

    // Parse job ID from "JOB <id> <payload>"
    // The server sends: "<id> <value>"
    size_t first_space = response.find(' ');
    if (first_space == string::npos) {
      cerr << "Error: Malformed job response" << endl;
      continue;
    }
    string id_str = response.substr(0, first_space);

    // Simulate work
    this_thread::sleep_for(chrono::seconds(1));

    string ack_msg = "ACK " + id_str + "\n";
    if (!send_all(sock, ack_msg))
      break;
  }

  close(sock);
  return 0;
}
