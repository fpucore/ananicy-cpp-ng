//
// Created by aviallon on 19/04/2021.
//

#include "core/rules.hpp"
#include "core/cgroups.hpp"

#include <filesystem> // for path, recursive_directory_iterator
#include <fstream> // for ifstream
#include <iostream> // for cout
#include <string> // for string, getline
#include <string_view> // for string_view

#include <fmt/format.h>

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

using nlohmann::json;

Rules::Rules(const fs::path &ruleset_directory, Config *config_src)
    : m_config(config_src) {
  if (!exists(ruleset_directory)) {
    spdlog::error(fmt::format("{}: directory {} does not exist", __func__,
                              std::string_view{ruleset_directory.c_str()}));
    return;
  }
  load_rules_from_directory(ruleset_directory);
}

bool Rules::load_rule_from_string(const std::string_view &line) {
  // NOTE: this technique makes it constexpr even in non constexpr context
  // Because since C++17 standard lambda functions are constexpr by default
  // @see https://en.cppreference.com/w/cpp/language/lambda
  const auto &is_invalid_str = [](auto &&s) {
    const auto &is_comment_str = [](auto &&s) {
      std::string_view hash_str{s};
      hash_str.remove_prefix(
          std::min(hash_str.find_first_not_of(' '), hash_str.size()));
      return hash_str.empty() || (hash_str[0] == '#');
    };
    return s.empty() || is_comment_str(s);
  };
  if (is_invalid_str(line)) {
    return false;
  }

  auto &&first_pair = line.find_first_of('{');
  auto &&second_pair = line.find_first_of('}');

  // Return if not found brackets in the given string.
  if (first_pair == std::string_view::npos ||
      second_pair == std::string_view::npos) {
    spdlog::error("Rule is invalid: pairs mismatch '}{'");
    return false;
  }

  // Remove trailing characters.
  auto &&clean_line =
      std::string_view{line.substr(first_pair).substr(0, second_pair + 1)};

  try {
    auto &&rule = json::parse(clean_line);

    if (rule.contains("name") && rule["name"].is_string()) { /* Program rule */
      const auto &rule_name = rule["name"];
      m_program_rules.insert_or_assign(rule_name, rule);
    } else if (rule.contains("type") &&
               rule["type"].is_string()) { /* Type rule */
      const auto &rule_type = rule["type"];
      m_type_rules.insert_or_assign(rule_type, rule);
    } else if (rule.contains("cgroup") &&
               rule["cgroup"].is_string()) { /* Cgroup rule */
      const auto &rule_cgroup = rule["cgroup"];
      m_cgroup_rules.insert_or_assign(rule_cgroup, rule);
    } else {
      spdlog::error(
          fmt::format("Rule does not have a name, or name is not a string: {}",
                      rule.dump()));
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::warn(
        fmt::format("Error parsing JSON: {}, line: {}", e.what(), line));
    return false;
  }

  return true;
}

void Rules::load_rules_from_file(const fs::path &path) {
  std::string   line{};
  std::ifstream rule_file{path};
  while (rule_file.good()) {
    std::getline(rule_file, line);
    load_rule_from_string(line);
  }
}

void Rules::show_all_rules() const noexcept {
  for (const auto &rule : m_cgroup_rules) {
    spdlog::debug(
        fmt::format("Cgroup: {}, {}", rule.first, rule.second.dump()));
  }
  for (const auto &rule : m_type_rules) {
    spdlog::debug(fmt::format("Type: {}, {}", rule.first, rule.second.dump()));
  }
  for (const auto &rule : m_program_rules) {
    spdlog::debug(fmt::format("Rule: {}, {}", rule.first, rule.second.dump()));
  }
}

void Rules::show_rules(rule_type type) const noexcept {
  const auto &rules = (type == rule_type::types)     ? m_type_rules
                      : (type == rule_type::cgroups) ? m_cgroup_rules
                                                     : m_program_rules;

  json rules_display;

  if (type == rule_type::rules) {
    for (const auto &rule : rules) {
      rules_display[rule.first] = get_rule(rule.first);
    }
  } else {
    for (const auto &rule : rules) {
      rules_display[rule.first] = rule.second;
    }
  }

  std::cout << rules_display.dump(4) << std::endl;
}

void Rules::load_rules_from_directory(const fs::path &dir_path) {
  const auto &is_valid_file = [](auto &&config_ref, auto &&file_ext) {
    return ((config_ref->cgroup_load() && file_ext.compare(".cgroups") == 0) ||
            (config_ref->type_load() && file_ext.compare(".types") == 0) ||
            (config_ref->rule_load() && file_ext.compare(".rules") == 0));
  };

  for (const auto &dir_entry : fs::recursive_directory_iterator{dir_path}) {
    if (fs::is_directory(dir_entry)) {
      spdlog::debug(fmt::format("Scanning directory {}",
                                std::string_view{dir_entry.path().c_str()}));
      continue;
    }

    if (is_valid_file(m_config, std::move(dir_entry.path().extension()))) {
      load_rules_from_file(dir_entry.path());
    }
  }
}

void Rules::create_cgroups() const noexcept {
  spdlog::info("Creating Cgroups...");
  for (const auto &cgroup_rule : m_cgroup_rules) {
    control_groups::create_cgroup(cgroup_rule.first);
    if (cgroup_rule.second.contains("CPUQuota")) {
      control_groups::set_cgroup_cpu_quota(
          cgroup_rule.first, cgroup_rule.second["CPUQuota"].get<unsigned>());
    }
  }
  spdlog::info("Finished creating Cgroups...");
}

json Rules::get_rule(const std::string &name) const noexcept {
  json rule;
  if (m_program_rules.contains(name)) {
    rule = m_program_rules.at(name);
    if (rule.contains("type") && m_type_rules.contains(rule["type"])) {
      json type_rule =
          m_type_rules.at(rule["type"]); // We create a copy of the type rule to
                                         // avoid overwriting it
      type_rule.merge_patch(
          rule); // We then merge the rule into it, overriding parameters from
                 // the type with those explicitly specified in the rule
      rule.merge_patch(type_rule); // We then merge it back into the rule
    }
  }
  return rule;
}

[[gnu::pure]] size_t Rules::size() const noexcept {
  return m_program_rules.size();
}
