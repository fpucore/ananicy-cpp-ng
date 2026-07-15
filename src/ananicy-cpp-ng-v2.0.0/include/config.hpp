//
// Created by aviallon on 20/04/2021.
//

#ifndef ANANICY_CPP_CONFIG_HPP
#define ANANICY_CPP_CONFIG_HPP

#include <chrono>        // for duration
#include <filesystem>    // for path
#include <optional>      // for optional
#include <string>        // for string
#include <string_view>   // for string_view
#include <unordered_map> // for unordered_map

#include <spdlog/spdlog.h>

class Config {
  using ConfigMap = std::unordered_map<std::string, std::string>;
  using ConfigInit =
      std::initializer_list<std::pair<std::string_view, std::string_view>>;

  static const ConfigInit &&default_config;

  inline bool check_rule(const std::string &rule_name,
                         const bool         default_value = true) {
    if (config.contains(rule_name))
      return config[rule_name] == "true";
    return default_value;
  }

public:
  explicit Config(const std::filesystem::path &config_path);

  bool apply_nice() { return check_rule("apply_nice"); }
  bool apply_latnice() { return check_rule("apply_latnice"); }

  bool apply_sched() { return check_rule("apply_sched"); }
  bool apply_ionice() { return check_rule("apply_ionice"); }
  bool apply_oom_score_adj() { return check_rule("apply_oom_score_adj"); }
  bool apply_cgroups() { return check_rule("apply_cgroup"); }
  bool log_applied_rule() { return check_rule("log_applied_rule"); }
  bool cgroup_load() { return check_rule("cgroup_load"); }
  bool type_load() { return check_rule("type_load"); }
  bool rule_load() { return check_rule("rule_load"); }
  std::chrono::duration<uint32_t, std::ratio<1, 1>> check_freq();

  bool cgroup_realtime_workaround() {
    return check_rule("cgroup_realtime_workaround", true);
  }

  spdlog::level::level_enum loglevel() {
    using spdlog::level::level_enum;
    if (config.contains("loglevel")) {
      if (config["loglevel"] == "trace") {
        return level_enum::trace;
      } else if (config["loglevel"] == "debug") {
        return level_enum::debug;
      } else if (config["loglevel"] == "info") {
        return level_enum::info;
      } else if (config["loglevel"] == "warn") {
        return level_enum::warn;
      } else if (config["loglevel"] == "error") {
        return level_enum::err;
      } else if (config["loglevel"] == "critical") {
        return level_enum::critical;
      } else {
        return level_enum::info;
      }
    }

    return level_enum::info;
  }

  void show();
  void reload() noexcept;
  void set(const std::string &key, const std::string &value);
  std::optional<std::string> get(const std::string &key) const;

  explicit operator std::string() const;

private:
  ConfigMap             config;
  std::filesystem::path m_config_path{};
};

#endif // ANANICY_CPP_CONFIG_HPP
