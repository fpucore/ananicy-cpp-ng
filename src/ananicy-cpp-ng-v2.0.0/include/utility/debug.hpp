#pragma once

#include <string>

namespace ananicy_debug {

void print_file(const std::string &path);

template <unsigned IssueNumber> void print_debug_for_issue();

template <> void print_debug_for_issue<21>();

} // namespace ananicy_debug