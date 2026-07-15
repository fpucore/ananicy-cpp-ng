#include <unistd.h>
//
// Created by aviallon on 13/04/2021.
//

#include "core/process.hpp"
#include "process_helpers.hpp"
#include "utility/synchronized_queue.hpp"
#include "utility/utils.hpp"

#if !defined(USE_BPF_PROC_IMPL)
#include <cerrno>
#endif

#include <filesystem> // for path
#include <fstream>    // for ifstream
#include <thread>     // for jthread

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

#if defined(USE_BPF_PROC_IMPL)
static volatile std::atomic<bool>   g_exiting;
static synchronized_queue<Process> *g_process_queue;

void ProcessQueue::init() noexcept {
  g_process_queue = &this->process_queue;
  this->full_scan();

  g_exiting.store(false, std::memory_order_relaxed);
  if (!m_bpf_program.initialize()) {
    spdlog::error("Failed to initialize the BPF program");
    m_status = false;
  }
}

extern "C" void handle_event(void *ctx, int32_t cpu, void *data,
                             uint32_t data_sz) {
  static constexpr auto TASK_COMM_LEN = 16;

  struct event {
    pid_t pid;
    pid_t prev_pid;
    __u64 delta_us;
    char  task[TASK_COMM_LEN];
  };

  auto *e = static_cast<struct event *>(data);

  /// Don't add process to the queue,
  /// if it's equal to previous one.
  /// @see: https://gitlab.com/ananicy-cpp/ananicy-cpp/-/issues/20
  if (e->pid == 0 || !g_process_queue || e->pid == e->prev_pid) {
    return;
  }

  std::string &&command = get_command_from_pid(e->pid);
  if (command.ends_with(".exe")) {
    std::replace(command.begin(), command.end(), '\\', '/');
    fs::path cmd_path{command.c_str()};
    command = cmd_path.filename().string();
  }

  Process process{
      .delta_us = e->delta_us, .pid = e->pid, .name = std::move(command)};
  spdlog::trace("Pushing new process: pid = {}", e->pid);
  g_process_queue->push(std::move(process));
}

extern "C" void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt) {
  fmt::print(stderr, "Lost {} events on CPU {}!\n", lost_cnt, cpu);
}

static std::int32_t handle_events(BPFProgram                  *bpf_program,
                                  synchronized_queue<Process> *queue,
                                  const std::stop_token       &stop_token) {

  std::int32_t status{};
  // while (!stop_token.stop_requested()) {
  while (!g_exiting.load(std::memory_order_consume)) {
    status = bpf_program->pool_event();
    if (status < 0 && status != -EINTR) {
      spdlog::error(fmt::format("Error received ({}): {}", status,
                                get_error_string(-status)));
      return EXIT_FAILURE;
    }
    /* reset err to return 0 if exiting */
    status = 0;
  }

  return status;
}

void ProcessQueue::start() {
  this->event_thread =
      std::jthread([this](const std::stop_token &stop_token) -> void {
        int ret = EXIT_FAILURE;
        while (ret == EXIT_FAILURE && !stop_token.stop_requested()) {
          spdlog::trace("Starting handle_events");

          ret = handle_events(&m_bpf_program, &this->process_queue, stop_token);

          if (ret == EXIT_SUCCESS || stop_token.stop_requested())
            break;

          spdlog::warn("restarting socket connection!");
          g_exiting.store(true, std::memory_order_relaxed);
          m_bpf_program.destroy();
          this->init();
        }
      });
}

#else

static std::int32_t handle_events(NetlinkProgram              *netlink_program,
                                  synchronized_queue<Process> *queue,
                                  const std::stop_token       &stop_token) {
  ssize_t status = 0;
  while (!stop_token.stop_requested()) {
    errno = 0;
    status = netlink_program->pool_message();
    const std::int32_t errnum = errno;
    if (status == 0) {
      return EXIT_SUCCESS;
    } else if (status == -1) {
      if (errnum == EINTR || errnum == EAGAIN) {
        continue;
      }
      if (errnum == ENOBUFS) {
        spdlog::warn(fmt::format("Received message overrun - ({})",
                                 get_error_string(errnum)));
        return EXIT_FAILURE;
      }
      spdlog::error(fmt::format("Bad netlink status ({}): {}", errnum,
                                get_error_string(errnum)));
      return EXIT_FAILURE;
    }

    /// Don't add process to the queue,
    /// if it's equal to the previous one.
    /// @see: https://gitlab.com/ananicy-cpp/ananicy-cpp/-/issues/20
    static pid_t prev_pid;

    const pid_t pid = netlink_program->get_pid();
    if (pid == 0 || pid == prev_pid) {
      continue;
    }
    prev_pid = pid;

    Process process{.pid = pid, .name = get_command_from_pid(pid)};
    spdlog::trace("Pushing new process: pid = {}", pid);
    queue->push(std::move(process));
  }

  return static_cast<std::int32_t>(status);
}

void ProcessQueue::init() noexcept {
  this->full_scan();

  if (!m_netlink_program.initialize()) {
    spdlog::error(fmt::format("Failed to initialize the NETLINK program : {}",
                              get_error_string(errno)));
    m_status = false;
  }
}

void ProcessQueue::start() {
  this->event_thread =
      std::jthread([this](const std::stop_token &stop_token) -> void {
        int ret = EXIT_FAILURE;
        while (ret == EXIT_FAILURE && !stop_token.stop_requested()) {
          spdlog::trace("Starting handle_events");

          ret = handle_events(&m_netlink_program, &this->process_queue,
                              stop_token);
          if (ret == EXIT_SUCCESS || stop_token.stop_requested())
            break;

          spdlog::warn("restarting socket connection!");
          m_netlink_program.destroy();
          this->init();
        };
      });
}

#endif

std::string get_command_from_pid(pid_t pid) {
  static std::uint32_t  exe_fail_count = 0;
  static constexpr auto COMMAND_NAME_HEURISTIC_SKIP_EXE_FAILURES = 5;

  // Cmdline method
  try {
    std::vector<std::string> cmdline = process_info::get_cmdline_from_pid(pid);
    if (!cmdline.empty()) {
      auto exe_name_begin = cmdline[0].find_last_of('/');

      spdlog::trace(fmt::format("{}: exe_name_begin: {}, cmdline: {}", __func__,
                                exe_name_begin, cmdline));

      if (exe_name_begin != std::string::npos) {
        return cmdline[0].substr(exe_name_begin + 1);
      }
      return cmdline[0];
    }

  } catch (const fs::filesystem_error &e) {
    spdlog::warn(fmt::format("{}: filesystem error: {} (path: {})", __func__,
                             e.what(), std::string_view{e.path1().c_str()}));
  }

  // Exe method
  /**
   * If reading the exe file doesn't work several times in a row,
   * it probably means we don't have the permissions for that.
   * We should skip it entirely for performance reasons.
   */
  if (exe_fail_count < COMMAND_NAME_HEURISTIC_SKIP_EXE_FAILURES) {
    try {
      const fs::path proc_path{fmt::format("/proc/{}/exe", pid)};

      const fs::path exe_path = read_symlink(proc_path);
      auto           exe_name = exe_path.filename().string();
      const auto     exe_name_end = exe_name.find(" (deleted)");
      if (exe_name_end != std::string::npos) {
        exe_name = exe_name.substr(0, exe_name_end + 1);
      }
      spdlog::trace("{}: exe filename: {}", __func__, exe_name);

      exe_fail_count = 0;

      return exe_name;
    } catch (const fs::filesystem_error &e) {
      const auto errcode = e.code().value();
      if (errcode == EEXIST)
        return "<unknown>";
      else if (errcode == EACCES || errcode == EPERM)
        exe_fail_count++;

      spdlog::trace(fmt::format("{}: read symlink failed: {}, code: {}",
                                __func__, e.code().message(), errcode));
    }
  }

  // Comm method
  try {
    std::ifstream comm_file(fmt::format("/proc/{}/comm", pid));
    std::string   comm;

    std::getline(comm_file, comm);
    return comm;
  } catch (const fs::filesystem_error &e) {
    spdlog::error(fmt::format("{}: filesystem error: {} (path: {})", __func__,
                              e.what(), std::string_view{e.path1().c_str()}));
  }

  return {"<unknown>"};
}

void ProcessQueue::full_scan() {
  spdlog::info("Doing a full scan");
  std::uint32_t n_process_found{};
  for (const auto &dir_entry : fs::directory_iterator("/proc")) {
    const auto &pid_str = dir_entry.path().filename().c_str();
    if (!isdigit(pid_str[0])) {
      continue;
    }
    const auto pid = to_int<std::int32_t>(pid_str);
    // spdlog::trace("Pushing existing process: {}", pid);
    auto &&command = get_command_from_pid(pid);

    this->process_queue.push(Process{
        .pid = pid,
        .name = std::move(command),
    });
    n_process_found++;
  }

  spdlog::debug("Full scan found {:d} processes", n_process_found);
}

void ProcessQueue::stop() noexcept {
#if defined(USE_BPF_PROC_IMPL)
  g_exiting.store(true, std::memory_order_relaxed);
#endif
  if (!this->event_thread.get_stop_token().stop_requested()) {
    this->event_thread.request_stop();
    this->event_thread.join();
  }
#if defined(USE_BPF_PROC_IMPL)
  m_bpf_program.destroy();
#else
  m_netlink_program.destroy();
#endif
}

pid_t get_pid() { return getpid(); }
