#include "netlink_program.hpp"

#include <csignal>
#include <cerrno>
#include <cstdio>
#include <atomic>
#include <string>
#include <vector>
#include <string_view>
#include <filesystem>

#include <fmt/core.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

static volatile std::atomic<bool> g_exiting;

static void sig_int(int) { g_exiting.store(true, std::memory_order_relaxed); }

std::string read_whole_file(const std::string_view& filepath) noexcept {
  // Use std::fopen because it's faster than std::ifstream
  auto* file = std::fopen(filepath.data(), "rb");
  if (file == nullptr) {
    spdlog::error("read failed: {}", std::strerror(errno));
    return {};
  }

  std::fseek(file, 0u, SEEK_END);
  const std::size_t size = static_cast<std::size_t>(std::ftell(file));
  std::fseek(file, 0u, SEEK_SET);

  std::string content;
  content.resize(size);

  const std::size_t read = std::fread(content.data(), sizeof(char), size, file);
  if (read != size) {
    spdlog::error("read failed: {}", std::strerror(errno));
    return {};
  }
  std::fclose(file);

  return content;
}

namespace process_info {
static std::vector<std::string> get_cmdline_from_pid(pid_t pid) noexcept {
  const std::string cmdline_path = fmt::format("/proc/{}/cmdline", pid);
  const std::string cmdline_value = read_whole_file(cmdline_path.c_str());
  std::vector<std::string> cmdline;

  size_t start_pos;
  size_t end_pos = 0;
  while ((start_pos = cmdline_value.find_first_not_of('\0', end_pos)) !=
         std::string::npos) {
    end_pos = cmdline_value.find('\0', start_pos);
    cmdline.push_back(cmdline_value.substr(start_pos, end_pos - start_pos));
  }

  return cmdline;
}
} // namespace process_info

std::string get_process_cmdline(pid_t pid) noexcept {
  // Cmdline method
  const std::vector<std::string> cmdline = process_info::get_cmdline_from_pid(pid);
  if (!cmdline.empty()) {
    auto exe_name_begin = cmdline[0].find_last_of('/');
    if (exe_name_begin != std::string::npos) {
      return cmdline[0].substr(exe_name_begin + 1);
    }
    return cmdline[0];
  }

  // Exe method
  /**
   * If reading the exe file doesn't work several times in a row,
   * it probably means we don't have the permissions for that.
   * We should skip it entirely for performance reasons.
   */
  static constexpr auto COMMAND_NAME_HEURISTIC_SKIP_EXE_FAILURES = 5;
  static std::uint32_t exe_fail_count = 0;
  if (exe_fail_count < COMMAND_NAME_HEURISTIC_SKIP_EXE_FAILURES) {
    try {
      const fs::path proc_path{fmt::format("/proc/{}/exe", pid)};

      const fs::path exe_path = fs::read_symlink(proc_path);
      std::string exe_name = exe_path.filename().string();
      const auto exe_name_end = exe_name.find(" (deleted)");
      if (exe_name_end != std::string::npos) {
        exe_name = exe_name.substr(0, exe_name_end + 1);
      }
      exe_fail_count = 0;

      return exe_name;
    } catch (const fs::filesystem_error &e) {
      const auto errcode = e.code().value();
      if (errcode == EEXIST)
        return "<unknown>";
      else if (errcode == EACCES || errcode == EPERM)
        exe_fail_count++;
    }
  }

  // Comm method
  const std::string proc_comm_path = fmt::format("/proc/{}/comm", pid);
  const std::string proc_comm = read_whole_file(std::string_view{proc_comm_path.c_str()});
  if (!proc_comm.empty()) {
    return proc_comm;
  }

  return {"<unknown>"};
}

void handle_event(const NetlinkProgram& program) {
  static pid_t prev_pid;
  pid_t pid = program.get_pid();
  if (pid == 0 || pid == prev_pid) {
    return;
  }
  prev_pid = pid;

  const std::uint64_t timestamp = program.get_timestamp();
  const std::string proc_cmd = get_process_cmdline(pid);
  fmt::print("({:<16}) {:<7} {:<14}\n", proc_cmd, pid, timestamp);
}

int main() {
  NetlinkProgram program;
  if (!program.initialize()) {
    spdlog::error("Failed to initialize the NETLINK program");
    return EXIT_FAILURE;
  }

  if (signal(SIGINT, sig_int) == SIG_ERR) {
    fmt::print(stderr, "can't set signal handler: {}\n", std::strerror(errno));
    program.destroy();
    return EXIT_FAILURE;
  }

  ssize_t status = 0;
  while (!g_exiting.load(std::memory_order_consume)) {
    errno = 0;
    status = program.pool_message();
    const std::int32_t errnum = errno;
    if (status == 0) {
      break;
    } else if (status == -1) {
      if (errnum == EINTR || errnum == EAGAIN) {
        continue;
      }
      if (errnum == ENOBUFS) {
        spdlog::warn("Received message overrun - ({})", std::strerror(errnum));
        program.destroy();
        return EXIT_FAILURE;
      }
      spdlog::error("Bad netlink status ({}): {}", errnum, std::strerror(errnum));
      program.destroy();
      return EXIT_FAILURE;
    }
    handle_event(program);
  }
  program.destroy();
  return EXIT_SUCCESS;
}
