#include <cstddef>
#include <iostream> // std::cout, std::cerr for printing to the terminal
#include <mutex>
#include <netinet/in.h> // sockaddr_in, htons(), INADDR_ANY
#include <string>
#include <sys/socket.h> // socket(), bind(), listen(), accept(), recv(), send()
#include <thread>
#include <unistd.h> // close() to close file descriptors

using namespace std;
const int PORT = 5003;

queue<string> jobs;
mutex counter_mutex;

void handle_client(int client_fd) {
  char data[1024];
  while (true) {
    ssize_t message = ::recv(client_fd, data, sizeof(data), 0);
    if (message <= 0) {
      ::close(client_fd);
      break;
    }

    string line(data, message);

    if (!line.empty() && line.back() == '\n') {
      line.pop_back();
    }
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    size_t sp = line.find(' ');
    string cmd = (sp == string::npos) ? line : line.substr(0, sp);
    string payload = (sp == string::npos) ? "" : line.substr(sp + 1);

    if (cmd == "SUBMIT") {
      if (payload.empty()) {
        continue;
      }
      lock_guard<mutex> lock(counter_mutex);
      jobs.push(payload);

    } else if (cmd == "REQUEST") {
      string value;
      {
        lock_guard<mutex> lock(counter_mutex);
        if (jobs.empty())
          value = "EMPTY";
        else {
          value = jobs.front();
          jobs.pop();
        }
      }
      string out = value + "\n";
      if (::send(client_fd, out.c_str(), out.size(), 0) == -1) {
        ::close(client_fd);
        break;
      }

    } else if (cmd == "QUIT") {
      ::close(client_fd);
      break;

    } else {
      cerr << "Invalid command" << endl;
      continue;
    }
  }
}

int main() {

  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    return 1;
  }

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

  if (::bind(server_fd, reinterpret_cast<sockaddr *>(&server_struct),
             sizeof(server_struct)) == -1) {
    ::close(server_fd);
    return 1;
  }

  if (::listen(server_fd, SOMAXCONN) == -1) {
    ::close(server_fd);
    return 1;
  }

  cout << "TCP Server Opened in localhost " << PORT << endl;

  sockaddr_in client_struct;

  while (true) {
    socklen_t len_client = sizeof(client_struct);
    int client_fd = ::accept(
        server_fd, reinterpret_cast<sockaddr *>(&client_struct), &len_client);

    if (client_fd == -1) {
      cerr << "Client couldn't be opened" << endl;
      continue;
    }

    // instead of forking, now we should create threads
    thread t1(handle_client, client_fd);
    t1.detach(); // makes a thread to be independent, which we want for this
                 // concurrent modelling
  }

  return 0;
}