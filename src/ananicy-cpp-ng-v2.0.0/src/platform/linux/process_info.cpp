#include "utility/process_info.hpp"
#include "core/process.hpp"
#include "process_helpers.hpp"
#include "syscalls.h"
#include "utility/utils.hpp"

#include <concepts>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace process_info {

template <std::integral T = std::size_t>
static constexpr inline std::string to_str(T number) {
  return fmt::format("{}", number);
}

static autogroup get_autogroup_from_str(const std::string &str) {
  const auto begin_group_num = str.find_first_not_of("/autogroup-");
  if (begin_group_num == std::string::npos) {
    return nullptr;
  }
  const auto end_group_num = str.find_first_of(' ', begin_group_num);
  const auto begin_nice_num = str.find_first_not_of("nice ", end_group_num);
  const auto end_nice_num = str.size();

  return {{"group", to_int(str.substr(begin_group_num, end_group_num))},
          {"nice", to_int(str.substr(begin_nice_num, end_nice_num))}};
}

nlohmann::json get_autogroup_map() {
  nlohmann::json autogroup_map = {};

  const auto &process_info_map = get_process_info_map();

  for (auto process_info : process_info_map) {
    if (process_info["autogroup"] != nullptr) {
      const auto autogroup_num =
          to_str(static_cast<std::int32_t>(process_info["autogroup"]["group"]));
      if (!autogroup_map.contains(autogroup_num)) {
        autogroup_map[autogroup_num.data()] = nlohmann::json::object();
        autogroup_map[autogroup_num.data()]["nice"] =
            process_info["autogroup"]["nice"];
        autogroup_map[autogroup_num.data()]["proc"] = nlohmann::json::object();
      }
      process_info.erase("autogroup");
      const auto pid = to_str(static_cast<std::int32_t>(process_info["tpid"]));
      autogroup_map[autogroup_num.data()]["proc"][pid.data()] = process_info;
    }
  }

  return autogroup_map;
}

static nlohmann::json parse_proc_status(process_id_t pid) {
  std::ifstream  status_file{fmt::format("/proc/{}/status", pid)};
  nlohmann::json status = nlohmann::json::object();

  std::string line;
  while (status_file.good()) {
    std::getline(status_file, line);
    const size_t      delimiter_pos = line.find_first_of(':');
    const std::string name = line.substr(0, delimiter_pos);
    std::string       value = line.substr(delimiter_pos + 1);

    const size_t first_non_whitespace = value.find_first_not_of({' ', '\t'});
    if (first_non_whitespace == std::string::npos) {
      value = "";
    } else {
      value = value.substr(first_non_whitespace); // Trim leading whitespace
    }
    status[name] = value;
  }

  return status;
}

static std::string get_sched_policy_name(unsigned sched_policy) {
  switch (sched_policy & 0xFF) {
  case SCHED_RR:
    return "rr";
  case SCHED_DEADLINE:
    return "deadline";
  case SCHED_FIFO:
    return "fifo";
  case SCHED_OTHER:
    return "normal";
  case SCHED_BATCH:
    return "batch";
  case SCHED_IDLE:
    return "idle";
  case SCHED_ISO:
    return "iso";
  default:
    return "unknown";
  }
}

static ananicy_sched_attr get_sched_attributes(process_id_t pid) {

  ::ananicy_sched_attr attr{};
  ananicy_sched_getattr(static_cast<pid_t>(pid), &attr, sizeof(attr), 0);

  return attr;
}

bool is_realtime(process_id_t pid) {
  const ananicy_sched_attr attr = get_sched_attributes(pid);
  return attr.sched_priority > 0;
}

bool is_kernel_process(process_id_t pid) {
  auto status = parse_proc_status(pid);
  return (status["PPid"] == "0") || (status["PPid"] == "2");
}

enum io_prio_class {
  NONE = IOPRIO_CLASS_NONE,
  REALTIME = IOPRIO_CLASS_RT,
  BEST_EFFORT = IOPRIO_CLASS_BE,
  IDLE = IOPRIO_CLASS_IDLE
};

struct io_prio_attributes {
  io_prio_class io_class;
  std::int32_t  io_nice;
};

static io_prio_attributes get_io_prio_attributes(process_id_t pid) {
  const std::int32_t io_prio_data = ioprio_get(IOPRIO_WHO_PROCESS, pid);
  const std::int32_t io_nice =
      IOPRIO_PRIO_DATA(static_cast<std::size_t>(io_prio_data));

  io_prio_class io_class;
  switch (IOPRIO_PRIO_CLASS(io_prio_data)) {
  case IOPRIO_CLASS_IDLE:
    io_class = IDLE;
    break;
  case IOPRIO_CLASS_BE:
    io_class = BEST_EFFORT;
    break;
  case IOPRIO_CLASS_RT:
    io_class = REALTIME;
    break;
  default:
    io_class = NONE;
    break;
  }

  return io_prio_attributes{.io_class = io_class, .io_nice = io_nice};
}

static std::optional<process_info> get_process_info(process_id_t pid) {
  if (!exists(fs::path(fmt::format("/proc/{}", pid))))
    return {};

  process_info info = nlohmann::json::object();

  try {
    std::error_code ec;

    nlohmann::json status = parse_proc_status(pid);
    if (status["PPid"] == 0) {
      spdlog::trace("{}: {} is a kernel thread, skipping", __func__, pid);
      return {};
    }

    info["pid"] = to_int(static_cast<std::string>(status["Tgid"]));

    info["tpid"] = pid;

    info["exe"] = fs::read_symlink(fmt::format("/proc/{}/exe", pid), ec);
    if (ec)
      info["exe"] = nullptr;

    std::string comm;
    std::getline(std::ifstream(fmt::format("/proc/{}/comm", pid)), comm);
    info["comm"] = comm;

    info["cmd"] = get_command_from_pid(static_cast<pid_t>(pid));

    std::string stat;
    std::getline(std::ifstream(fmt::format("/proc/{}/stat", pid)), stat);
    info["stat"] = stat;

    const auto stat_name_begin = stat.find_first_of('(') + 1;
    const auto stat_name_end = stat.find_first_of(')');

    info["stat_name"] =
        stat.substr(stat_name_begin, stat_name_end - stat_name_begin);

    std::string autogroup_value;
    std::getline(std::ifstream(fmt::format("/proc/{}/autogroup", pid)),
                 autogroup_value);
    info["autogroup"] = get_autogroup_from_str(autogroup_value);

    const auto sched_info = get_sched_attributes(pid);

    info["sched"] = get_sched_policy_name(sched_info.sched_policy);
    info["rtprio"] = uint32_t(sched_info.sched_priority);
    info["nice"] = int32_t(sched_info.sched_nice);
    info["latency_nice"] = 0;

    const auto io_data = get_io_prio_attributes(pid);

    info["ionice"] = nlohmann::json::array();

    std::string io_class_name{};
    switch (io_data.io_class) {
    case NONE:
      io_class_name = "none";
      break;
    case REALTIME:
      io_class_name = "realtime";
      break;
    case BEST_EFFORT:
      io_class_name = "best-effort";
      break;
    case IDLE:
      io_class_name = "idle";
      break;
    }

    info["ionice"][0] = io_class_name;
    if (io_data.io_class == BEST_EFFORT) {
      info["ionice"][1] = io_data.io_nice;
    } else {
      info["ionice"][1] = nullptr;
    }

    unsigned oom_score_adj = 0;
    std::ifstream{fmt::format("/proc/{}/oom_score_adj", pid)} >> oom_score_adj;
    info["oom_score_adj"] = oom_score_adj;

    info["cmdline"] = get_cmdline_from_pid(static_cast<int32_t>(pid));

  } catch (const fs::filesystem_error &e) {
    spdlog::trace(fmt::format("{}: Error getting {} infos: {} ({}, {})",
                              __func__, static_cast<int32_t>(pid), e.what(),
                              std::string_view{e.path1().c_str()},
                              std::string_view{e.path2().c_str()}));

    info["incomplete"] = true;
  } catch (const std::exception &e) {
    spdlog::trace(fmt::format("{:s}: Error getting {:d} infos: {}", __func__,
                              pid, e.what()));
    info["incomplete"] = true;
  }

  return {info};
}

nlohmann::json get_process_info_map() {
  nlohmann::json process_map = nlohmann::json::object();

  const auto &is_valid_entry = [&process_map](auto &&entry_path) {
    const auto &entry_filename = entry_path.filename().c_str();
    if (!isdigit(entry_filename[0])) {
      spdlog::trace(fmt::format("{}: Skipping entry '{}'", __func__,
                                std::string_view{entry_path.c_str()}));
      return false;
    }

    if (entry_filename == "0" || process_map.contains(entry_filename)) {
      return false;
    }

    return true;
  };

  for (const auto &dir_entry : fs::directory_iterator("/proc")) {
    const auto &entry_path = dir_entry.path();
    if (!is_valid_entry(entry_path)) {
      continue;
    }

    try {
      const auto &tasks_dir = fmt::format("{}/task", entry_path.c_str());
      for (const auto &dir_nested_entry : fs::directory_iterator(tasks_dir)) {
        const auto &nested_entry_path = dir_nested_entry.path();
        if (!is_valid_entry(nested_entry_path)) {
          continue;
        }
        const auto &nested_entry_filename =
            nested_entry_path.filename().c_str();

        const auto pid = to_int<std::uint32_t>(nested_entry_filename);
        const auto proc_info = get_process_info(pid);
        if (proc_info.has_value()) {
          process_map[nested_entry_filename] = proc_info.value();
        }
      }
    } catch (...) {
    }
  }

  return process_map;
}
} // namespace process_info
