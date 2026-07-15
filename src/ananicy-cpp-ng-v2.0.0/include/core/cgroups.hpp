#pragma once

#include <sys/types.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace control_groups {
struct cgroup_info {
  std::filesystem::path path;
  enum cgroup_version { none = 0, v1 = 1, v2 = 2 } version;
};
int         create_cgroup(const std::string_view &name);
int         set_cgroup_cpu_quota(const std::string &name, unsigned quota);
int         add_pid_to_cgroup(pid_t pid, const std::string &cgroup_name);
cgroup_info get_cgroup_version(bool reset = false);
std::string get_cgroup_for_pid(pid_t pid);

} // namespace control_groups
