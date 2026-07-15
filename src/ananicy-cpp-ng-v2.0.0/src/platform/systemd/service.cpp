#include <unistd.h>
#include "service.hpp"

#include <systemd/sd-daemon.h>
#include <systemd/sd-login.h>

#include <spdlog/spdlog.h>

namespace service {
status current_status = STARTING;

void set_status(service::status status) {
  current_status = status;
  switch (status) {
  case STARTING:
    break;
  case STARTED:
    sd_notify(0, "READY=1");
    break;
  case STOPPING:
    sd_notify(0, "STOPPING=1");
    break;
  case STOPPED:
    break;
  }
}

[[gnu::pure]] status get_status() { return current_status; }

void reload() { spdlog::warn("{}: Not implemented yet", __func__); }

void stop() { spdlog::warn("{}: Not implemented yet", __func__); }

std::string get_unit_name() {
  char *unit_name;
  errno = 0;
  sd_pid_get_unit(getpid(), &unit_name);
  if (errno == ENODATA) {
    return "<empty>";
  }

  auto ret = std::string(unit_name);
  free(unit_name);

  return ret;
}

} // namespace service
