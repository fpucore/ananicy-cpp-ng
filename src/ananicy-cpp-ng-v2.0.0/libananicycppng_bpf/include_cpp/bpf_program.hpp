#ifndef BPF_PROGRAM_HPP
#define BPF_PROGRAM_HPP

#include <cstdint>

#include <bpf/libbpf.h>

#include "bpf_program_utils.h"

class BPFProgram {
public:
  /// Default constructor.
  BPFProgram() = default;
  BPFProgram(std::uint64_t min_us, bool verbose) noexcept
      : m_min_us(min_us), m_verbose(verbose) {}
  ~BPFProgram() = default;

  constexpr BPFProgram(BPFProgram &&program) = default;
  constexpr BPFProgram &operator=(BPFProgram &&program) = default;

  // Only use if you want to re-initialize.
  // Overwise use constructor.
  inline bool initialize() noexcept {
    if ((m_bpf_program != nullptr) || (m_perf_buffer != nullptr)) {
      return false;
    }

    m_bpf_program = initialize_bpf_program(m_min_us, m_verbose);
    m_perf_buffer = bpf_program_init_events(m_bpf_program);
    if ((m_bpf_program == nullptr) || (m_perf_buffer == nullptr)) {
      destroy_bpf_program(m_bpf_program, m_perf_buffer);
      return false;
    }

    return true;
  }

  // Can be if you want to destroy the owned objects.
  inline void destroy() noexcept {
    destroy_bpf_program(m_bpf_program, m_perf_buffer);

    m_bpf_program = nullptr;
    m_perf_buffer = nullptr;
  }
  int pool_event() noexcept { return perf_buffer__poll(m_perf_buffer, 100); }

  perf_buffer *get_perf_buffer() noexcept { return m_perf_buffer; }

  //
  // deleted
  constexpr BPFProgram(const BPFProgram &program) = delete;
  constexpr BPFProgram &operator=(const BPFProgram &program) = delete;

private:
  bool                    m_verbose{false};
  ananicy_cpp_ng_bpf_t    *m_bpf_program{nullptr};
  perf_buffer             *m_perf_buffer{nullptr};
  std::uint64_t           m_min_us{30};
};

#endif // BPF_PROGRAM_HPP
