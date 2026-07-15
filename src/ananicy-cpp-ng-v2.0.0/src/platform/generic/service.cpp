#include "service.hpp"

#include <spdlog/spdlog.h>

namespace service {
status current_status = STARTING;

void set_status(service::status status) { current_status = status; }

status get_status() { return current_status; }

void reload() { spdlog::warn("{}: Not implemented yet", __func__); }

void stop() { spdlog::warn("{}: Not implemented yet", __func__); }

std::string get_unit_name() { return "<not using systemd>"; }
} // namespace service
