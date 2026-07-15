#ifndef NETLINK_PROGRAM_UTILS_H
#define NETLINK_PROGRAM_UTILS_H

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <sys/types.h>

typedef struct ananicy_cpp_ng_netlink ananicy_cpp_ng_netlink_t;

#ifdef __cplusplus
extern "C" {
#endif

int32_t connect_netlink_socket();
int32_t set_event_subscription(int32_t nl_sock, bool enable);

ananicy_cpp_ng_netlink_t* initialize_netlink_program();
void destroy_netlink_program(ananicy_cpp_ng_netlink_t* obj);

uint64_t netlink_program_get_timestamp(ananicy_cpp_ng_netlink_t* obj);
pid_t netlink_program_get_pid(ananicy_cpp_ng_netlink_t* obj);
ssize_t netlink_program_pool_message(ananicy_cpp_ng_netlink_t* obj);

#ifdef __cplusplus
}
#endif

#endif // NETLINK_PROGRAM_UTILS_H
