#include <cstdint>
#include "utility/backtrace.hpp"

#include <cstdlib> // for free
#include <cxxabi.h> // for abi::__cxa_demangle
#include <execinfo.h> // for backtrace, backtrace_symbols

#include <exception> // for current_exception, exception_ptr
#include <iostream> // for cerr

#include <fmt/format.h>

static constexpr std::int32_t BACKTRACE_LENGTH = 20;

namespace backtrace_handler {

void print_trace() noexcept {
  void  *backtrace_buffer[BACKTRACE_LENGTH];
  int    size = backtrace(backtrace_buffer, BACKTRACE_LENGTH);
  char **lines = backtrace_symbols(backtrace_buffer, size);
  if (lines != nullptr) {
    std::cerr << fmt::format("Obtained {} stack frames.\n", size);
    for (int i = 0; i < size; ++i) {
      std::cerr << fmt::format("({}) {}\n", i, lines[i]);
    }
  }

  // NOTE: You have to call here `free` function directly
  // because of delete/free their allocators does not
  // have exactly same behaviour
  free(lines);
}

[[noreturn]] static void terminate_handler() noexcept(false) {
  // set the exception pointer in case of an exception
  std::exception_ptr exception_ptr = std::current_exception();

  try {
    // if exception was thrown - rethrow it
    if (exception_ptr) {
      std::rethrow_exception(exception_ptr);
    }
  } catch (const std::exception &e) {
    std::cerr << fmt::format("{} caught unhandled exception: {} {}",
                             __PRETTY_FUNCTION__,
                             abi::__cxa_demangle(typeid(e).name(), nullptr,
                                                 nullptr, nullptr),
                             e.what())
              << std::endl;
  } catch (...) {
    std::cerr << fmt::format("{} caught unknown/unhandled exception.",
                             __PRETTY_FUNCTION__)
              << std::endl;
  }

  print_trace();
  std::abort();
}

[[maybe_unused]] static const bool set_terminate =
    std::set_terminate(terminate_handler);

} // namespace backtrace_handler
