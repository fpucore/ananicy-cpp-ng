//
// Created by aviallon on 13/04/2021.
//

#ifndef ANANICY_CPP_PROCESS_HPP
#define ANANICY_CPP_PROCESS_HPP

#if defined(USE_BPF_PROC_IMPL)
#include "bpf_program.hpp"
#else
#include "netlink_program.hpp"
#endif
#include "utility/synchronized_queue.hpp"

#include <string>
#include <thread>

pid_t get_pid();

std::string get_command_from_pid(pid_t pid);

struct Process {
#if defined(USE_BPF_PROC_IMPL)
  uint64_t delta_us{};
#endif
  pid_t       pid{};
  std::string name{};

#if defined(USE_BPF_PROC_IMPL)
  constexpr inline bool operator==(const Process &other) const {
    return (delta_us == other.delta_us) && (pid == other.pid) &&
           (name == other.name);
  }
#else
  constexpr inline bool operator==(const Process &other) const {
    return (pid == other.pid) && (name == other.name);
  }
#endif
  constexpr inline bool operator!=(const Process &other) const {
    return !(*this == other);
  }
};

class ProcessQueue {
private:
  void init() noexcept;
#if defined(USE_BPF_PROC_IMPL)
  bool       m_verbose{false};
  uint64_t   m_min_us{60};
  BPFProgram m_bpf_program{};

#else
  NetlinkProgram m_netlink_program{};
#endif
  std::jthread event_thread;
  bool         m_status{true};

public:
#if defined(USE_BPF_PROC_IMPL)
  ProcessQueue(uint64_t min_us, bool verbose)
      : m_min_us(min_us), m_verbose(verbose),
        m_bpf_program(m_min_us, m_verbose) {
    this->init();
  }
#else
  ProcessQueue() { this->init(); }
#endif
  ~ProcessQueue() noexcept { this->stop(); }

  void start();
  void stop() noexcept;

  bool status() noexcept { return m_status; }

  void full_scan();

#ifdef USE_EXPERIMENTAL_IMPL
  synchronized_queue<Process> process_queue{INT16_MAX};
#else
  synchronized_queue<Process> process_queue;
#endif
};

#endif // ANANICY_CPP_PROCESS_HPP
