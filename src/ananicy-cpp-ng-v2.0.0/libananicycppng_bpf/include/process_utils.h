#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include <stdint.h>
#include <sys/types.h>
#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void handle_event(void* ctx, int32_t cpu, void* data, uint32_t data_sz);
void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt);

#ifdef __cplusplus
}
#endif

#endif // PROCESS_UTILS_H
