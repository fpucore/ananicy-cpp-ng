#define _XOPEN_SOURCE       700 // NOLINT(bugprone-reserved-identifier)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include "config.hpp"
#include "core/cgroups.hpp"
#include "core/priority.hpp"
#include "core/process.hpp"
#include "core/rules.hpp"
#include "core/worker.hpp"
#include "service.hpp"
#include "utility/argument_parser.hpp"
#include "utility/debug.hpp"
#include "utility/process_info.hpp"
#include "utility/singleton_process.hpp"
#include "utility/utils.hpp"
#include "version.hpp"

#include <cerrno>
#include <csignal>

#include <iostream>
#include <string_view>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

using std::filesystem::path;

int main(const int argc, const char **argv) {
  if constexpr (BUILD_TYPE == "Debug") {
    spdlog::set_level(spdlog::level::trace);
  } else {
    spdlog::set_level(spdlog::level::info);
  }

  const auto myuid = getuid();
  const auto start_time = std::chrono::system_clock::now();

  argparse::argument_parser parser(
      "ananicy-cpp-ng" /*, "ANother Auto NIce daemon, fully written in C++"*/);

  parser.add_argument()
      .name("--verbose")
      .name("-v")
      .description("Increase verbosity level");
  parser.add_argument().name("--help").name("-h").description(
      "Print this help and exit");
  parser.add_argument()
      .name("--version")
      .description("Print version information");
  parser.add_argument()
      .name("--force-remove-semaphore")
      .description("Don't use it, unless you know what you are doing.");
  parser.add_argument()
      .name("--reload")
      .description("Reload configuration/rules");
  parser.add_argument()
      .name("--benchmark")
      .description("Should we activate benchmark mode or not");
  parser.add_argument()
      .name("--benchmark-count")
      .type(typeid(uint32_t))
      .description("How many pids to process before stopping");

#if defined(USE_BPF_PROC_IMPL)
  parser.add_argument()
      .name("--bpf-min-us")
      .type(typeid(uint64_t))
      .description("Trace latency");
#endif

  parser.add_argument()
      .name("--manual-scanning")
      .name("--manualscanning")
      .description(
          "Scan for all processes at a regular interval by reading the "
          "procfs.\n"
          "Not recommended unless ananicy-cpp-ng doesn't work as expected.");

  parser.add_argument().name("action").description(
      "What action to perform. Valid actions are:\n"
      "dump [sub-action]\n"
      "start");

  parser.add_argument()
      .name("sub-action")
      .description("What sub-action to perform. Valid sub-actions are:\n"
                   "rules, types, cgroups, proc, autogroup\n");

  argparse::argparse_error err = parser.parse_args(argc, argv);
  if (err) {
    spdlog::critical(fmt::format("Argument parsing error: {}", err.what()));
    parser.print_help();
    return EXIT_FAILURE;
  }

  if (parser["help"] || (argc < 2)) {
    parser.print_help();
    return EXIT_SUCCESS;
  }

  if (parser["version"]) {
    std::cout << fmt::format("ananicy-cpp-ng {}\n", VERSION);
    return EXIT_SUCCESS;
  }

  SingletonProcess singleton("/ananicy-cpp-ngMutex");
  if (parser["force-remove-semaphore"]) {
    if (!SingletonProcess::remove(singleton.get_name())) {
      std::cerr << fmt::format("Failed to remove semaphore! msg '{}'\n",
                               std::string_view{strerror(errno)});
      return EXIT_FAILURE;
    }
    std::cerr << "Semaphore was successfully removed!\n";
    return EXIT_SUCCESS;
  }

  if (parser["reload"]) {
    if (!singleton.try_open()) {
      std::cerr << "Unable to reload. ananicy-cpp-ng is not running!\n";
      return EXIT_FAILURE;
    }
    singleton.try_write("RELOAD");
    return EXIT_SUCCESS;
  }

  const std::string config_dir_path =
      get_env("ANANICY_CPP_NG_CONFDIR").value_or("/etc/ananicy.d");
  const std::string config_path =
      get_env("ANANICY_CPP_NG_CONF").value_or("/etc/ananicy.d/ananicy.conf");

  Config conf(config_path);
  conf.show();

  spdlog::set_level(conf.loglevel());

  if (parser["verbose"]) {
    spdlog::debug("Verbose flag set!");
    const int loglevel =
        std::max(0, (spdlog::get_level() - 1)) % spdlog::level::n_levels;
    spdlog::set_level(static_cast<spdlog::level::level_enum>(loglevel));
  }

  if (parser["benchmark"]) {
    spdlog::warn("Benchmark enabled!");
  }

  auto benchmark_count = parser["benchmark-count"].value<uint32_t>();
  if (benchmark_count.has_value()) {
    spdlog::warn("Benchmark count: {}", benchmark_count.value());
  }

  if (parser["manual-scanning"]) {
    spdlog::info("Manual scanning enabled! Increasing Ananicy Nice value to "
                 "prevent lag.");
    priority::set_priority(get_pid(), 19);
    spdlog::info(
        fmt::format("Checking frequency set to {}", conf.check_freq()));
  }

  std::cout << fmt::format("ananicy-cpp-ng {}\n", VERSION) << std::flush;

  Rules rules(config_dir_path, &conf);

  const std::optional<std::string> action = parser["action"];
  const std::optional<std::string> sub_action = parser["sub-action"];
  if (!action.has_value()) {
    spdlog::critical("No action requested!");
    parser.print_help();
    std::exit(EXIT_FAILURE);
  } else if (action == "start") {
    service::set_status(service::STARTING);
    spdlog::info("Starting ananicy-cpp-ng");
  } else if (action == "dump") {
    if (!sub_action.has_value()) {
      spdlog::error("A sub-action must be specified for {}.", action.value());
      std::exit(EXIT_FAILURE);
    } else if (sub_action == "rules") {
      rules.show_rules(Rules::rule_type::rules);
    } else if (sub_action == "types") {
      rules.show_rules(Rules::rule_type::types);
    } else if (sub_action == "cgroups") {
      rules.show_rules(Rules::rule_type::cgroups);
    } else if (sub_action == "proc") {
      std::cout << process_info::get_process_info_map().dump(4) << std::endl;
    } else if (sub_action == "autogroup") {
      std::cout << process_info::get_autogroup_map().dump(4) << std::endl;
    } else {
      spdlog::critical("Unknown sub-action for dump: {}", sub_action.value());
      std::exit(EXIT_FAILURE);
    }
    std::exit(EXIT_SUCCESS);
  } else if (action == "debug") {
    spdlog::set_level(spdlog::level::trace);
    if (!sub_action.has_value()) {
      spdlog::error("A sub-action must be specified for {}.", action.value());
      std::exit(EXIT_FAILURE);
    } else if (sub_action == "cgroups") {
      ananicy_debug::print_debug_for_issue<21>();
    }
    std::exit(EXIT_SUCCESS);
  } else {
    spdlog::critical("Unknown action requested: {}", action.value());
  }

  // Check if we have sufficient permission for the requested operation
  if (myuid > 0) {
    service::set_status(service::STOPPING);
    std::cerr << "You cannot perform this operation unless you are root!\n";
    return EXIT_FAILURE;
  }

  if (!singleton.try_create()) {
    service::set_status(service::STOPPING);
    std::cerr << "ananicy-cpp-ng is already running!\n";
    return EXIT_FAILURE;
  }

  singleton.start_read_queue([&] { conf.reload(); });

  rules.show_all_rules();

  control_groups::get_cgroup_version();
  rules.create_cgroups();

  InterruptHandler ih({SIGINT, SIGTERM});

#if defined(USE_BPF_PROC_IMPL)
  const uint64_t bpf_min_us =
      parser["bpf-min-us"].value<uint64_t>().value_or(60);
  ProcessQueue processListener(bpf_min_us, parser["verbose"]);
#else
  ProcessQueue processListener;
#endif

  if (!processListener.status()) {
    service::set_status(service::STOPPING);
    std::cerr << "processListener initialization failed!\n";
    SingletonProcess::remove(singleton.get_name());
    ih.stop();
    return EXIT_FAILURE;
  }

  processListener.start();

  Worker worker(&rules, &conf, &processListener.process_queue);
  worker.start();

  if (conf.cgroup_realtime_workaround()) {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
    spdlog::debug("Cgroup realtime workaround requested, re-checking cgroups");
    control_groups::get_cgroup_version(true);
    rules.create_cgroups();
  }

  service::set_status(service::STARTED);

  while (!ih.should_exit()) {
    if (parser["benchmark"]) {
      std::this_thread::sleep_for(30s);
    } else {
      ih.wait_for(conf.check_freq());
    }

    if (ih.should_exit() || parser["benchmark"] ||
        (benchmark_count.has_value() &&
         worker.processed_processes() >= benchmark_count))
      break;

    if (parser["manual-scanning"]) {
      spdlog::info("Starting full-scan");
      processListener.full_scan();
    }
    spdlog::debug(
        fmt::format("Processed processes: {}", worker.processed_processes()));
  }
  spdlog::info("Stopping ananicy-cpp-ng...");
  service::set_status(service::STOPPING);

  // Close shared memory before removing it.
  singleton.close_shm();
  SingletonProcess::remove(singleton.get_name());

  processListener.stop();
  spdlog::debug("Stopped process listener");

  worker.stop();
  spdlog::debug("Stopped process worker");

  ih.stop();

  const std::chrono::duration<double, std::ratio<1, 1>> duration =
      std::chrono::system_clock::now() - start_time;

  spdlog::info(fmt::format(
      "Summary:\n{} processes processed, ran for {:%H:%M:%S} seconds",
      worker.processed_processes(), duration));

  return EXIT_SUCCESS;
}
