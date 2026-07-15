#include "config.hpp"
#include "core/rules.hpp"

#include <memory>
#include <string_view>

#include <spdlog/spdlog.h>

static constexpr std::string_view config_path =
    (TEST_DIR "test-rulesconfig.txt");

static std::unique_ptr<Config> g_conf{nullptr};
static std::unique_ptr<Rules> g_rules{nullptr};

void do_initialization() noexcept {
  spdlog::set_level(spdlog::level::off);
  if (!g_conf) {
      g_conf = std::make_unique<Config>(config_path);
  }
  if (!g_rules) {
      g_rules = std::make_unique<Rules>("", g_conf.get());
  }
}

// see http://llvm.org/docs/LibFuzzer.html
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, std::size_t size) {
  do_initialization();

  std::string_view data_str{reinterpret_cast<const char *>(data), size};
  g_rules->load_rule_from_string(data_str);

  return 0;
}
