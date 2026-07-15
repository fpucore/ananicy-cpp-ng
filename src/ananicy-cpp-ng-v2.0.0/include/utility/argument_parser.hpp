#pragma once

#include <atomic>
#include <concepts>
#include <map>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <spdlog/spdlog.h>

namespace argparse {

class argparse_error {
private:
  std::optional<std::string> error_message = {};

public:
  argparse_error() = default;
  explicit(false) argparse_error(std::string message)
      : error_message(std::move(message)){};
  explicit(false) operator bool() const { return error_message.has_value(); }
  [[nodiscard]] std::string what() const { return error_message.value_or(""); }
};

class argument_value {
private:
  std::optional<std::string> m_value;
  std::type_index            m_type;

public:
  argument_value(std::optional<std::string> value, std::type_index type)
      : m_value(std::move(value)), m_type(type) {}

  operator std::optional<std::string>() const { return m_value; }

  template <std::integral T> std::optional<T> value() const {
    spdlog::debug(fmt::format("{}: m_value = {}", __func__, m_value.value_or("N/A")));

    if (!m_value.has_value())
      return {};

    if constexpr (std::signed_integral<T>) {
      return std::stoll(m_value.value());
    } else {
      return std::stoull(m_value.value());
    }
  }

  operator bool() const;
};

class argument {
  static std::atomic<unsigned> _id;

  std::string                m_name{};
  std::vector<char>          m_short_names{};
  std::vector<std::string>   m_long_names{};
  std::string                m_description{};
  std::optional<std::string> m_value{};
  std::type_index            m_value_type = typeid(bool);
  bool                       m_is_positional = false;
  bool                       m_is_flag = true;

public:
  const unsigned id;

  argument() : id(_id++) { m_name = "<undefined>"; };
  explicit(false) argument(const std::string &name);

  argument &positional();
  argument &description(const std::string &description);
  argument &type(const std::type_info &type);
  argument &name(const std::string &name);

  void set_value(const std::string &value);

  [[nodiscard]] std::vector<std::string>   get_long_names() const;
  [[nodiscard]] std::vector<char>          get_short_names() const;
  [[nodiscard]] std::string                get_name() const;
  [[nodiscard]] std::type_index            get_type() const;
  [[nodiscard]] std::optional<std::string> get_value() const;
  [[nodiscard]] std::optional<std::string> get_description() const;

  [[nodiscard]] bool is_positional() const;
  [[nodiscard]] bool is_flag() const;
  [[nodiscard]] bool is_defined() const;

  [[nodiscard]] operator std::string() const;

#if (__cpp_constexpr >= 202002) ||                                             \
    (defined(__clang__) && (__cpp_constexpr >= 201907))
  constexpr
#else
  inline
#endif
  friend bool
  operator==(const argument &arg, const std::string &name) {
    return arg.m_name == name;
  }
};

class argument_parser {
  std::map<unsigned, argument> m_args{};
  std::string                  m_program_name{};

public:
  argument_parser(std::string program_name = "");
  argparse_error parse_args(const std::vector<std::string> &args);
  argparse_error parse_args(int argc, const char **argv);

  argument &add_argument();
  argument &add_argument(const argument &argument);
  argument &add_argument(const std::string &name);

  std::optional<argument> get_arg(const std::string &name);
  std::string             get_program_name();

  void print_help();

  argument_value operator[](const std::string &name);

  template <typename T>
  std::optional<T> operator[](const std::string &name) = delete;
};
} // namespace argparse

template <> struct fmt::formatter<argparse::argument> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx);

  template <typename FormatContext>
  constexpr auto format(const argparse::argument &arg, FormatContext &ctx);
};

template <typename ParseContext>
constexpr auto fmt::formatter<argparse::argument>::parse(ParseContext &ctx) {
  return ctx.begin();
}

template <typename FormatContext>
constexpr auto
fmt::formatter<argparse::argument>::format(const argparse::argument &arg,
                                           FormatContext            &ctx) {
  return fmt::format_to(ctx.out(), arg.operator std::string());
}
