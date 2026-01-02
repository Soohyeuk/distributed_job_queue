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

int counter = 0;
mutex counter_mutex;

void handle_client(int client_fd) {
  char data[1024];
  while (true) {
    ssize_t message = ::recv(client_fd, data, sizeof(data), 0);

    if (!(message > 0)) {
      ::close(client_fd);
      break;
    }

    string input(data, message);
    if (input == "INC\n") {
      lock_guard<mutex> lock(counter_mutex); // mutex to stop race condition
      counter++;

    } else if (input == "GET\n") {
      unique_lock<mutex> lock(counter_mutex);
      int value = counter;
      lock.unlock();

      string out = to_string(value) + "\n";
      int sending = ::send(client_fd, out.c_str(), out.size(), 0);

      if (sending == -1) {
        ::close(client_fd);
        cerr << "Sending had an issue" << endl;
        break;
      }

      cout << "Message sent for GET request" << endl;

    } else if (input == "QUIT\n") {
      ::close(client_fd);
      cout << "Client closed" << endl;
      break;

    } else {
      cerr << "Invalid Argument" << endl;
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