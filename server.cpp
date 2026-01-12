#include <cstddef>
#include <cstdint>
#include <fstream>
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

void write_ahead_log(Job job, string type) {
  ofstream log_file("write-ahead.log", ios::app);
  if (type == "ADD") {
    log_file << "ADD " << job.job_id << " " << job.job_text << "\n";
  } else if (type == "DONE") {
    log_file << "DONE " << job.job_id << "\n";
  }
  log_file.flush();
  log_file.close();
}

void read_ahead_log() {
  ifstream log_file("write-ahead.log");
  string line;
  unordered_map<uint64_t, Job> temp_jobs;

  while (getline(log_file, line)) {
    size_t sp = line.find(' ');
    string cmd = (sp == string::npos) ? line : line.substr(0, sp);
    string payload = (sp == string::npos) ? "" : line.substr(sp + 1);

    if (cmd == "ADD") {
      Job job;
      size_t sp2 = payload.find(' ');
      if (sp2 != string::npos) {
        job.job_id = stoull(payload.substr(0, sp2));
        job.job_text = payload.substr(sp2 + 1);
        temp_jobs[job.job_id] = job;

        if (job.job_id > job_id)
          job_id = job.job_id;
      }

    } else if (cmd == "DONE") {
      try {
        uint64_t id = stoull(payload);
        temp_jobs.erase(id);
      } catch (...) {
      }
    }
  }
  log_file.close();

  for (auto &pair : temp_jobs) {
    jobs.push(pair.second);
  }
  cout << "Recovered " << temp_jobs.size() << " jobs from WAL." << endl;
}

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
      write_ahead_log(jobs.back(), "ADD");

    } else if (cmd == "REQUEST") {
      string out;

      // mutex should start and end in this bracket
      {
        lock_guard<mutex> lock(job_mutex);
        if (jobs.empty()) {
          out = "EMPTY\n";
        } else {
          uint64_t id = jobs.front().job_id;
          string value = jobs.front().job_text;
          inflight[client_fd] = jobs.front();
          jobs.pop();
          out = to_string(id) + " " + value + "\n";
        }
      }
      if (::send(client_fd, out.c_str(), out.size(), 0) == -1) {
        handle_inflight_request(client_fd);
        ::close(client_fd);
        break;
      }

    } else if (cmd == "QUIT") {
      handle_inflight_request(client_fd);
      ::close(client_fd);
      break;

    } else if (cmd == "ACK") {
      uint64_t id = 0;
      try {
        id = stoull(payload);
      } catch (...) {
        cerr << "Invalid ACK format" << endl;
        continue;
      }

      lock_guard<mutex> lock(job_mutex);
      auto it = inflight.find(client_fd);
      if (it != inflight.end() && it->second.job_id == id) {
        cout << "Job " << id << " ACKed by client " << client_fd << endl;
        inflight.erase(it);
      } else {
        cerr << "received ACK for unknown job or client " << client_fd << endl;
      }
      write_ahead_log({id, ""}, "DONE");

    } else if (cmd == "FAIL") {
      uint64_t id = 0;
      try {
        id = stoull(payload);
      } catch (...) {
        cerr << "Invalid FAIL format" << endl;
        continue;
      }

      lock_guard<mutex> lock(job_mutex);
      auto it = inflight.find(client_fd);
      if (it != inflight.end() && it->second.job_id == id) {
        cout << "Job " << id << " FAILED by client " << client_fd
             << ", requeuing." << endl;
        jobs.push(it->second);
        inflight.erase(it);
      }

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
  read_ahead_log();

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
