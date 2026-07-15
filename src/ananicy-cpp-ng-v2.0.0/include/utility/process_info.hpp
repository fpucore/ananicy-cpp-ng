#pragma once

#include <map>
#include <vector>

#include <nlohmann/json.hpp>

namespace process_info {
using autogroup = nlohmann::json;

using process_info = nlohmann::json;
using process_id_t = unsigned;

bool is_realtime(process_id_t pid);
bool is_kernel_process(process_id_t pid);

nlohmann::json get_autogroup_map();
nlohmann::json get_process_info_map();
} // namespace process_info
