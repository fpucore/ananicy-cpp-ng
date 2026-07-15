//
// Created by aviallon on 20/04/2021.
//

#include "core/priority.hpp"
#include "syscalls.h"
#include "utility/utils.hpp"

#include <filesystem>  // for directory_iterator
#include <fstream>     // for ofstream
#include <string_view> // for string_view

#include <fmt/format.h>

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

static std::int32_t test_errno(std::int32_t            errcode,
                               const std::string_view &func_name, pid_t pid) {
  if (errcode == 0) {
    spdlog::debug("{}: Successfully applied to {}", func_name, pid);
    return 1;
  }

  if (errcode == ESRCH) {
    spdlog::warn("{}: Process not found: {}", func_name, pid);
    return 0;
  } else if (errcode == EACCES || errcode == EPERM) {
    throw std::runtime_error(fmt::format(
        "{}: Insufficient permission to set io-priority of process {}",
        func_name, pid));
  }
  spdlog::error(fmt::format("{}: Unknown error, errno is {}({})", func_name,
                            std::string_view{strerror(errcode)}, errcode));
  return -1;
}

namespace priority {

bool set_priority(pid_t pid, std::int32_t nice_value) {
  std::int32_t errcode = 0;
  const auto  &task_path = fmt::format("/proc/{}/task", pid);
  try {
    for (const auto &dir_entry : fs::directory_iterator(task_path)) {
      const auto tid =
          to_int<std::int32_t>(dir_entry.path().filename().c_str());
      errno = 0;
      if (tid > 0)
        setpriority(PRIO_PROCESS, static_cast<id_t>(tid), nice_value);
      errcode = errno;
    }
  } catch (const fs::filesystem_error &e) {
    spdlog::debug(fmt::format("{}: filesystem_error: {} (path: {})", __func__,
                              e.what(), std::string_view{e.path1().c_str()}));
    return 0;
  } catch (const std::exception &e) {
    spdlog::critical(
        fmt::format("{}: unknown exception: {}", __func__, e.what()));
  }

  return test_errno(errcode, __func__, pid);
}

bool set_latency_nice(pid_t pid, std::int32_t latency_nice_value) {
  std::int32_t errcode = 0;
  const auto  &task_path = fmt::format("/proc/{}/task", pid);
  try {
    for (const auto &dir_entry : fs::directory_iterator(task_path)) {
      const auto tid =
          to_int<std::int32_t>(dir_entry.path().filename().c_str());
      errno = 0;
      if (tid > 0)
        set_latnice(tid, latency_nice_value);
      errcode = errno;
    }
  } catch (const fs::filesystem_error &e) {
    spdlog::debug(fmt::format("{}: filesystem_error: {} (path: {})", __func__,
                              e.what(), std::string_view{e.path1().c_str()}));
    return 0;
  } catch (const std::exception &e) {
    spdlog::critical(
        fmt::format("{}: unknown exception: {}", __func__, e.what()));
  }

  return test_errno(errcode, __func__, pid);
}

std::int32_t get_latency_nice(pid_t pid) { return get_latnice(pid); }

bool set_io_priority(pid_t pid, const std::string_view &io_class,
                     std::int32_t value) {
  std::int32_t io_class_value = 0;
  if (io_class == "best-effort") {
    io_class_value = IOPRIO_CLASS_BE;
  } else if (io_class == "realtime") {
    io_class_value = IOPRIO_CLASS_RT;
  } else if (io_class == "idle") {
    io_class_value = IOPRIO_CLASS_IDLE;
  } else if (io_class == "none") {
    io_class_value = IOPRIO_CLASS_NONE;
  } else {
    spdlog::error("Unknown io class {}, skipping...", io_class);
    return true;
  }

  const std::int32_t io_prio = IOPRIO_PRIO_VALUE(io_class_value, value);

  if (!ioprio_valid(io_prio)) {
    spdlog::error("IO priority is invalid, skipping...");
    return true;
  }

  errno = 0;
  ioprio_set(IOPRIO_WHO_PROCESS, static_cast<id_t>(pid), io_prio);

  return test_errno(errno, __func__, pid);
}

bool set_sched(pid_t pid, const std::string_view &sched_name,
               unsigned rt_prio) {
  std::int32_t sched = 0;
  sched_param  param = {};
  if (sched_name == "idle") {
    sched = SCHED_IDLE;
  } else if (sched_name == "normal" || sched_name == "other") {
    sched = SCHED_OTHER;
  } else if (sched_name == "rr") {
    sched = SCHED_RR;
    param.sched_priority = static_cast<std::int32_t>(rt_prio);
  } else if (sched_name == "fifo") {
    sched = SCHED_FIFO;
    param.sched_priority = static_cast<std::int32_t>(rt_prio);
  } else if (sched_name == "deadline") {
    spdlog::warn("deadline scheduler is not available yet (due to its "
                 "different behavior)");
    //    sched = SCHED_DEADLINE;
    //    param.sched_priority = 1;
  } else if (sched_name == "batch") {
    sched = SCHED_BATCH;
  } else {
    spdlog::error("Invalid scheduler '{}'", __func__, sched_name);
    return false;
  }
  errno = 0;
  sched_setscheduler(pid, sched, &param);

  const std::int32_t success = test_errno(errno, __func__, pid);
  if (success == -1)
    spdlog::error("{}: Error info: sched_name: {}, pid: {}", __func__,
                  sched_name, pid);

  return success;
}

bool set_oom_score_adjust(pid_t pid, std::int32_t value) {

  try {
    const auto oom_score_path{fmt::format("/proc/{}/oom_score_adj", pid)};

    std::ofstream oom_score_file{oom_score_path};
    if (oom_score_file.fail()) {
      spdlog::error("{}: Couldn't set OOM score adjustment to {} for {}",
                    __func__, value, pid);
      return false;
    }

    oom_score_file << value;
  } catch (const fs::filesystem_error &e) {
    spdlog::error(fmt::format("{}: Unknown filesystem error: {} (path: {})",
                              __func__, e.what(),
                              std::string_view{e.path1().c_str()}));
    return false;
  }

  return true;
}

std::string_view test_latnice_support() noexcept {
  pid_t        pid{0};
  std::int32_t latency_nice{-20};

  const std::int32_t saved_latnice = get_latnice(pid);
  errno = 0;

  // Use this here instead of function,
  // to suppress errors from function call.
  struct ananicy_sched_attr attr = {
      .size = sizeof(struct ananicy_sched_attr),
      .sched_flags = SCHED_FLAG_LATENCY_NICE | SCHED_FLAG_KEEP_PARAMS,
      .sched_latency_nice = latency_nice,
  };
  const std::int32_t err = ananicy_sched_setattr(pid, &attr, 0);
  bool               is_supported{};
  if (err == 0 && errno == 0) {
    set_latnice(pid, saved_latnice);
    is_supported = true;
  }
  return (is_supported) ? "true" : "false";
}

} // namespace priority
