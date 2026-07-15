#include "core/cgroups.hpp"

#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

// Helper function which guarantees we target a systemd .slice unit
static std::string format_slice_name(const std::string_view &name) {
    std::string slice_name(name);
    if (slice_name.find(".slice") == std::string::npos) {
        slice_name += ".slice";
    }
    return slice_name;
}

std::int32_t control_groups::create_cgroup(const std::string_view &cgroup_name) {
    // Under systemd DBus mgmt, slices are created implicitly when properties 
    // are set or when transient scopes are assigned to them
    // Manual dir creation is now obsolete
    spdlog::trace("create_cgroup bypassed: Systemd dynamically manages slice {}", cgroup_name);
    return 0;
}

std::int32_t control_groups::set_cgroup_cpu_quota(const std::string &name, std::uint32_t quota) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus *bus = nullptr;
    
    int r = sd_bus_default_system(&bus);
    if (r < 0) {
        spdlog::error("Failed to connect to system bus: {}", strerror(-r));
        return 1;
    }

    std::string slice_name = format_slice_name(name);
    
    // systemd DBus expects CPUQuotaPerSecUSec in microseconds per second
    // 100% CPU = 1,000,000 us. Multiply the quota percentage by 10,000
    uint64_t quota_usec = static_cast<uint64_t>(std::clamp(quota, 0u, 100u)) * 10000ULL;

    // Issue DBus call to dynamically set/update the slice properties in memory
    r = sd_bus_call_method(bus,
                           "org.freedesktop.systemd1",
                           "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager",
                           "SetUnitProperties",
                           &error,
                           nullptr,
                           "sba(sv)",
                           slice_name.c_str(),
                           1, // runtime mode (true, lost on reboot)
                           1, // size of properties array
                           "CPUQuotaPerSecUSec", "t", quota_usec);

    if (r < 0) {
        spdlog::error("Systemd refused to set quota for {}: {}", slice_name, error.message);
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        return 1;
    }

    spdlog::debug("Set {} CPUQuota to {}% successfully via DBus", slice_name, quota);
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return 0;
}

int32_t control_groups::add_pid_to_cgroup(pid_t pid, const std::string &cgroup_name) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus *bus = nullptr;
    
    int r = sd_bus_default_system(&bus);
    if (r < 0) {
         spdlog::error("Failed to connect to system bus: {}", strerror(-r));
         return 1;
    }

    std::string slice_name = format_slice_name(cgroup_name);
    std::string scope_name = fmt::format("ananicy-ng-{}.scope", pid);

    // Tell systemd to wrap this specific PID in a transient scope, 
    // and assign that scope to the target slice
    r = sd_bus_call_method(bus,
                           "org.freedesktop.systemd1",
                           "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager",
                           "StartTransientUnit",
                           &error,
                           nullptr,
                           "ssa(sv)a(sa(sv))",
                           scope_name.c_str(),
                           "fail", // Job mode
                           2,      // Number of properties
                           "PIDs", "au", 1, pid,
                           "Slice", "s", slice_name.c_str(),
                           0);     // Empty aux array

    if (r < 0) {
        spdlog::error("Systemd refused to route PID {} to {}: {}", pid, slice_name, error.message);
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        return 1;
    }

    spdlog::debug("Successfully routed PID {} to {}", pid, slice_name);
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return 0;
}

control_groups::cgroup_info control_groups::get_cgroup_version(bool reset) {
    // We have entirely bypassed manual filesystem parsing
    // Return a dummy v2 struct to satisfy the rest of the application
    return {.path = "/sys/fs/cgroup", .version = cgroup_info::cgroup_version::v2};
}

std::string control_groups::get_cgroup_for_pid(pid_t pid) {
    char *cgroup = nullptr;
    
    // Utilize systemd-login to ask the system exactly where this PID resides
    if (sd_pid_get_cgroup(pid, &cgroup) >= 0) {
        std::string cgroup_name(cgroup);
        free(cgroup);
        return cgroup_name;
    }
    
    return "<empty>";
}
