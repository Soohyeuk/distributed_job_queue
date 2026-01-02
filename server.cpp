#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

using namespace std;
const int PORT = 5003;

struct Job {
  uint64_t job_id;
  string job_text;
};

queue<Job> jobs;
unordered_map<int, Job> inflight;
uint64_t job_id = 0;
mutex job_mutex;

void handle_inflight_request(int client_fd) {
  // we want to ensure that if a job is incomplete, but the client disconnects
  // prematurely, the job still belongs to the queue without losing it
  lock_guard<mutex> lock(job_mutex);
  auto temp_job = inflight.find(client_fd);
  if (temp_job != inflight.end()) {
    jobs.push(temp_job->second);
    inflight.erase(temp_job);
  }
}

void handle_client(int client_fd) {
  char data[1024];
  while (true) {
    ssize_t message = ::recv(client_fd, data, sizeof(data), 0);

    if (message < 0) {
      cerr << "Error receiving a message" << endl;
      handle_inflight_request(client_fd);
      ::close(client_fd);
      break;
    } else if (message == 0) {
      ::close(client_fd);
      break;
    }

    // parsing command submitted by the client
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
      cout << cmd << " " << payload << endl;
      lock_guard<mutex> lock(job_mutex);
      jobs.push({++job_id, payload});

    } else if (cmd == "REQUEST") {
      uint64_t id;
      string value;

      // mutex should start and end in this bracket
      {
        lock_guard<mutex> lock(job_mutex);
        if (jobs.empty())
          value = "EMPTY";
        else {
          id = jobs.front().job_id;
          value = jobs.front().job_text;
          inflight[client_fd] = jobs.front();
          jobs.pop();
        }
      }

      // send the result back
      string out = to_string(id) + " " + value + "\n";
      if (::send(client_fd, out.c_str(), out.size(), 0) == -1) {
        handle_inflight_request(client_fd);
        ::close(client_fd);
        break;
      }

    } else if (cmd == "QUIT") {
      handle_inflight_request(client_fd);
      ::close(client_fd);
      break;

    } else {
      cerr << "Invalid command " << line << endl;
      continue;
    }
  }
};

int main() {
  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    cerr << "Server not running" << endl;
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
                            .sin_port = htons(static_cast<uint16_t>(PORT))};

  sockaddr_in client_struct;

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

  while (true) {
    socklen_t len_client = sizeof(client_struct);
    int client_fd = ::accept(
        server_fd, reinterpret_cast<sockaddr *>(&client_struct), &len_client);

    if (client_fd == -1) {
      cerr << "Client connection failed" << endl;
      continue;
    }

    thread t1(handle_client, client_fd);
    t1.detach();
  }
}
