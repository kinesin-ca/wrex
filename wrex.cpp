#include <cstring>
#include <iomanip>
#include <iostream>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <syslog.h>
#include <thread>
#include <unistd.h>
#include <vector>

class CommandLogger {
private:
  std::string current_user;
  std::string tty_name;
  std::string hostname;

  void initialize_metadata() {
    // Get current user
    struct passwd *pw = getpwuid(getuid());
    current_user = pw ? pw->pw_name : "unknown";

    // Get TTY
    char *tty = ttyname(STDIN_FILENO);
    tty_name = tty ? tty : "unknown";

    // Get hostname
    char host_buffer[256];
    if (gethostname(host_buffer, sizeof(host_buffer)) == 0) {
      hostname = host_buffer;
    } else {
      hostname = "unknown";
    }

    // Open syslog
    openlog("wrex", LOG_PID | LOG_CONS, LOG_USER);
  }

  std::string escape_json_string(const std::string &input) {
    std::ostringstream escaped;
    for (char c : input) {
      switch (c) {
      case '"':
        escaped << "\\\"";
        break;
      case '\\':
        escaped << "\\\\";
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        if (c < 32 || c > 126) {
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                  << (unsigned char)c;
        } else {
          escaped << c;
        }
        break;
      }
    }
    return escaped.str();
  }

  void log_line(const std::string &line, const std::string &fd_type,
                pid_t child_pid) {
    // Create JSON record
    std::ostringstream json;
    json << "{"
         << "\"user\":\"" << escape_json_string(current_user) << "\","
         << "\"pid\":" << child_pid << ","
         << "\"tty\":\"" << escape_json_string(tty_name) << "\","
         << "\"fd\":\"" << fd_type << "\","
         << "\"host\":\"" << escape_json_string(hostname) << "\","
         << "\"message\":\"" << escape_json_string(line) << "\","
         << "\"timestamp\":" << time(nullptr) << "}";

    // Send to syslog
    syslog(LOG_INFO, "%s", json.str().c_str());
  }

  void read_pipe(int pipe_fd, const std::string &fd_type, pid_t child_pid) {
    FILE *fp = fdopen(pipe_fd, "r");
    if (!fp) {
      perror("fdopen");
      return;
    }

    char *line = nullptr;
    size_t len = 0;
    ssize_t read;

    auto &tty = (fd_type == "stdout" ? std::cout : std::cerr);

    while ((read = getline(&line, &len, fp)) != -1) {
      // Remove trailing newline if present
      if (read > 0 && line[read - 1] == '\n') {
        line[read - 1] = '\0';
      }

      log_line(std::string(line), fd_type, child_pid);
      tty << line << std::endl;
    }

    if (line) {
      free(line);
    }
    fclose(fp);
  }

public:
  CommandLogger() { initialize_metadata(); }

  ~CommandLogger() { closelog(); }

  int execute_command(const std::vector<std::string> &args) {
    if (args.empty()) {
      std::cerr << "No command specified" << std::endl;
      return 1;
    }

    // Create pipes for stdout and stderr
    int stdout_pipe[2], stderr_pipe[2];

    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
      perror("pipe");
      return 1;
    }

    pid_t child_pid = fork();

    if (child_pid == -1) {
      perror("fork");
      close(stdout_pipe[0]);
      close(stdout_pipe[1]);
      close(stderr_pipe[0]);
      close(stderr_pipe[1]);
      return 1;
    }

    if (child_pid == 0) {
      // Child process
      close(stdout_pipe[0]); // Close read end
      close(stderr_pipe[0]); // Close read end

      // Redirect stdout and stderr to pipes
      dup2(stdout_pipe[1], STDOUT_FILENO);
      dup2(stderr_pipe[1], STDERR_FILENO);

      close(stdout_pipe[1]);
      close(stderr_pipe[1]);

      // Prepare arguments for execvp
      std::vector<char *> c_args;
      for (const auto &arg : args) {
        c_args.push_back(const_cast<char *>(arg.c_str()));
      }
      c_args.push_back(nullptr);

      // Execute the command
      execvp(c_args[0], c_args.data());
      perror("execvp");
      exit(127);
    } else {
      // Parent process
      close(stdout_pipe[1]); // Close write end
      close(stderr_pipe[1]); // Close write end

      // Log the command executed
      {
        std::ostringstream ss;
        ss << "CMD: ";
        for (const auto &arg : args) {
          ss << ' ' << arg;
        }

        log_line(ss.str(), "meta", child_pid);
      }

      // Create threads to read from both pipes
      std::thread stdout_reader(&CommandLogger::read_pipe, this, stdout_pipe[0],
                                "stdout", child_pid);
      std::thread stderr_reader(&CommandLogger::read_pipe, this, stderr_pipe[0],
                                "stderr", child_pid);

      // Wait for child process to complete
      int status;
      waitpid(child_pid, &status, 0);

      // Wait for reader threads to complete
      stdout_reader.join();
      stderr_reader.join();

      close(stdout_pipe[0]);
      close(stderr_pipe[0]);

      {
        std::ostringstream ss;
        ss << "RC: " << status;
        log_line(ss.str(), "meta", child_pid);
      }

      return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
  }
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
    std::cerr << "Example: " << argv[0] << " ls -la /tmp" << std::endl;
    return 1;
  }

  // Convert command line arguments to vector
  std::vector<std::string> command_args;
  for (int i = 1; i < argc; i++) {
    command_args.push_back(argv[i]);
  }

  CommandLogger logger;
  return logger.execute_command(command_args);
}
