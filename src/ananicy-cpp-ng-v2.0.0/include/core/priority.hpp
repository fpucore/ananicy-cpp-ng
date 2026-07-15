//
// Created by aviallon on 20/04/2021.
//

#ifndef ANANICY_CPP_PRIORITY_HPP
#define ANANICY_CPP_PRIORITY_HPP

#include <sys/types.h>

#include <string>
#include <string_view>

namespace priority {

bool set_priority(pid_t pid, int nice_value);

bool set_latency_nice(pid_t pid, int latency_nice_value);

int get_latency_nice(pid_t pid);

bool set_io_priority(pid_t pid, const std::string_view &io_class, int value);

bool set_sched(pid_t pid, const std::string_view &sched_name, unsigned rt_prio);

bool set_oom_score_adjust(pid_t pid, int value);

std::string_view test_latnice_support() noexcept;

} // namespace priority

#endif // ANANICY_CPP_PRIORITY_HPP
