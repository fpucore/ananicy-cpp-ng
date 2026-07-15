//
// Created by aviallon on 13/04/2021.
//

#ifndef ANANICY_CPP_UTILS_HPP
#define ANANICY_CPP_UTILS_HPP

#include <atomic>
#include <charconv>
#include <concepts>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

class InterruptHandler {
public:
  InterruptHandler(std::initializer_list<std::int32_t> &&signals);

  template <class Rep, class Period>
  inline void wait_for(std::chrono::duration<Rep, Period> duration) {
    std::unique_lock<std::mutex> lock(m_exit_mutex);
    m_exit_condition.wait_for(lock, duration);
  }

  [[nodiscard]] bool should_exit() const { return m_need_exit; }
  void               stop();

private:
  std::condition_variable m_exit_condition;
  std::mutex              m_exit_mutex;

  // NOTE: do we really need to use volatile here?
  volatile std::atomic_bool m_need_exit;
};

std::optional<std::string> get_env(const std::string &name);

std::string_view get_error_string(std::int32_t error_number);

std::string read_file(const std::string &path);

template <std::integral T = std::size_t>
constexpr inline T to_int(const std::string_view &str) {
  T result = 0;
  std::from_chars(str.data(), str.data() + str.size(), result);
  return result;
}

#endif // ANANICY_CPP_UTILS_HPP
