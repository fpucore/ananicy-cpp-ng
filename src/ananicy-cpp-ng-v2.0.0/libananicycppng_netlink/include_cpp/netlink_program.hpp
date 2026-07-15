#ifndef NETLINK_PROGRAM_HPP
#define NETLINK_PROGRAM_HPP

#include <cstdint>

#include "netlink_program_utils.h"

class NetlinkProgram {
public:
  /// Default constructor.
  NetlinkProgram() = default;
  ~NetlinkProgram() = default;

  constexpr NetlinkProgram(NetlinkProgram &&program) = default;
  constexpr NetlinkProgram &operator=(NetlinkProgram &&program) = default;

  // Only use if you want to re-initialize.
  // Overwise use constructor.
  inline bool initialize() noexcept {
    if (m_netlink_program != nullptr) {
      return false;
    }

    m_netlink_program = initialize_netlink_program();
    if (m_netlink_program == nullptr) {
      destroy_netlink_program(m_netlink_program);
      return false;
    }
    return true;
  }

  // Can be if you want to destroy the owned objects.
  inline void destroy() noexcept {
    destroy_netlink_program(m_netlink_program);
    m_netlink_program = nullptr;
  }
  ssize_t pool_message() noexcept { return netlink_program_pool_message(m_netlink_program); }

  pid_t get_pid() const noexcept { return netlink_program_get_pid(m_netlink_program); }
  uint64_t get_timestamp() const noexcept { return netlink_program_get_timestamp(m_netlink_program); }

  //
  // deleted
  constexpr NetlinkProgram(const NetlinkProgram &program) = delete;
  constexpr NetlinkProgram &operator=(const NetlinkProgram &program) = delete;

private:
  ananicy_cpp_ng_netlink_t *m_netlink_program{nullptr};
};

#endif // NETLINK_PROGRAM_HPP
