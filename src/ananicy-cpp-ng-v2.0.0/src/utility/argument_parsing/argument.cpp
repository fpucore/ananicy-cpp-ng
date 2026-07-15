#include <cstring>
#include "utility/argument_parser.hpp"

#include <cstdlib>
#include <cxxabi.h>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include <fmt/ranges.h>

namespace argparse {
std::atomic<unsigned> argument::_id = 0;

namespace {
// NOTE: abi::__cxa_demangle requires to free memory.
// Also, abi::__cxa_demangle can return nullptr; std::string{nullptr} is
// undefined behaviour
std::string demangle_symbol(const std::type_index &type_index) {
  std::size_t                            sz{128};
  std::unique_ptr<char, decltype(&free)> buffer(
      static_cast<char *>(std::malloc(sz)), free);
  std::memset(buffer.get(), 0, sz);
  int32_t   status{};
  char *demangle_name =
      abi::__cxa_demangle(type_index.name(), buffer.get(), &sz, &status);
  if (!demangle_name) {
    return "<unknown symbol>";
  }
  return demangle_name;
}
} // namespace

argument &argument::positional() {
  m_is_positional = true;
  return *this;
}

argument::argument(const std::string &name_value) : id(_id++) {
  name(name_value);
}

argument &argument::description(const std::string &desc) {
  m_description = desc;

  return *this;
}

std::vector<std::string> argument::get_long_names() const {
  return m_long_names;
}

std::vector<char> argument::get_short_names() const { return m_short_names; }

[[gnu::pure]] std::type_index argument::get_type() const { return m_value_type; }

argument &argument::type(const std::type_info &type) {
  m_value_type = type;
  m_is_flag = false;
  return *this;
}

void argument::set_value(const std::string &value) { m_value = value; }

[[gnu::pure]] bool argument::is_positional() const { return m_is_positional; }

[[gnu::pure]] bool argument::is_flag() const { return m_is_flag; }

bool argument::is_defined() const { return m_value.has_value(); }

std::optional<std::string> argument::get_value() const { return m_value; }

std::string argument::get_name() const { return m_name; }

argument &argument::name(const std::string &name_value) {
  if (name_value.empty()) {
    throw std::runtime_error("name can't be empty");
  }

  std::string actual_name{};

  if (name_value[0] != '-') {
    m_is_positional = true;
    actual_name = name_value;
  } else if (name_value[0] == '-' && name_value[1] != '-') {
    actual_name = name_value[1];
    m_short_names.emplace_back(actual_name[0]);
  } else {
    //    std::cout << "New long argument with name: " << name_value <<
    //    std::endl;
    actual_name = name_value.substr(2);
    m_long_names.emplace_back(actual_name);
  }
  if (m_name.empty() || m_name == "<undefined>") {
    //    std::cout << "Setting " << id << " name to " << actual_name <<
    //    std::endl;
    m_name = actual_name;
  }

  return *this;
}

argument::operator std::string() const {
  return fmt::format("Argument-{}: {} {}, long: {}, short: {}, value: {}", id,
                     demangle_symbol(m_value_type), m_name, get_long_names(),
                     get_short_names(), get_value().value_or("<undefined>"));
}

std::optional<std::string> argument::get_description() const {
  if (m_description.empty())
    return {};
  return m_description;
}

} // namespace argparse
