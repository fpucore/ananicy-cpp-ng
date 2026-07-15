#pragma once

#include <string>
namespace service {
enum status {
  STARTING,
  STARTED,
  STOPPING,
  STOPPED,
};

void   set_status(status status);
status get_status();

std::string get_unit_name();

void reload();
void stop();
} // namespace service
