#include <argp.h>
#include <csignal>

#include <atomic>
#include <charconv>
#include <string_view>

#include "bpf_program.hpp"

#include <fmt/core.h>

#include <spdlog/spdlog.h>

static volatile std::atomic<bool> g_exiting;

struct env {
  __u64 min_us;
  bool  verbose;
} env = {
    .min_us = 10000UL,
};

const char argp_program_doc[] =
    "Trace high run queue latency.\n"
    "\n"
    "USAGE: runqslower_cpp [--help] [min_us]\n"
    "\n"
    "EXAMPLES:\n"
    "    runqslower_cpp         # trace latency higher than 10000 us "
    "(default)\n"
    "    runqslower_cpp 1000    # trace latency higher than 1000 us\n";

static const struct argp_option opts[] = {
    {"verbose", 'v', nullptr, 0, "Verbose debug output"},
    {nullptr, 'h', nullptr, OPTION_HIDDEN, "Show the full help"},
    {},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state) {
  static int pos_args;

  switch (key) {
  case 'h':
    argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
    break;
  case 'v':
    env.verbose = true;
    break;
  case ARGP_KEY_ARG: {
    const std::string_view arg_str = arg;
    if (pos_args++) {
      fmt::print(stderr, "Unrecognized positional argument: {}\n", arg_str);
      argp_usage(state);
    }

    unsigned long long min_us;
    if (std::from_chars(arg_str.data(), arg_str.data() + arg_str.size(), min_us)
            .ec != std::errc()) {
      fmt::print(stderr, "Invalid delay (in us): {}\n", arg_str);
      argp_usage(state);
    }
    env.min_us = min_us;
    break;
  }
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

extern "C" void handle_event(void *ctx, int32_t cpu, void *data,
                             uint32_t data_sz) {
  static constexpr auto TASK_COMM_LEN = 16;

  struct event {
    pid_t pid;
    pid_t prev_pid;
    __u64 delta_us;
    char  task[TASK_COMM_LEN];
  };

  auto *e = static_cast<struct event *>(data);
  if (e->pid == 0 || e->pid == e->prev_pid) {
    return;
  }

  fmt::print("({:<16}) {:<7} {:<14}\n", e->task, e->pid, e->delta_us);
}

extern "C" void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt) {
  fmt::print(stderr, "Lost {} events on CPU {}!\n", lost_cnt, cpu);
}

static void sig_int(int) { g_exiting.store(true, std::memory_order_relaxed); }

int main(const int argc, char **argv) {
  static const struct argp argp = {
      .options = opts,
      .parser = parse_arg,
      .doc = argp_program_doc,
  };
  int err = argp_parse(&argp, argc, argv, 0, nullptr, nullptr);
  if (err)
    return err;

  if (env.verbose) {
    spdlog::set_level(spdlog::level::debug);
  } else {
    spdlog::set_level(spdlog::level::info);
  }

  BPFProgram program(env.min_us, env.verbose);
  if (!program.initialize()) {
    spdlog::error("Failed to initialize the BPF program");
    return EXIT_FAILURE;
  }

  if (signal(SIGINT, sig_int) == SIG_ERR) {
    fmt::print(stderr, "can't set signal handler: {}\n", std::strerror(errno));
    program.destroy();
    return EXIT_FAILURE;
  }

  fmt::print("Tracing run queue latency higher than {} us\n", env.min_us);
  fmt::print("{:<18} {:<7} {:<14}\n", "COMM", "TID", "LAT(us)");

  int err_code = 0;
  while (!g_exiting.load(std::memory_order_consume)) {
    err_code = program.pool_event();
    if (err_code < 0 && err_code != -EINTR) {
      spdlog::error("Error received ({}): {}", err_code,
                    std::strerror(-err_code));
      program.destroy();
      return EXIT_FAILURE;
    }
    /* reset err to return 0 if exiting */
    err_code = 0;
  }
  return err_code;
}
