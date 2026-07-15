#ifndef BPF_PROGRAM_UTILS_H
#define BPF_PROGRAM_UTILS_H

#include <stdint.h>
#include <bpf/libbpf.h>

typedef struct ananicy_cpp_ng_bpf ananicy_cpp_ng_bpf_t;

#ifdef __cplusplus
extern "C" {
#endif

ananicy_cpp_ng_bpf_t* initialize_bpf_program(uint64_t min_us, bool verbose);
void destroy_bpf_program(ananicy_cpp_ng_bpf_t* obj, struct perf_buffer* pb);

struct perf_buffer* bpf_program_init_events(ananicy_cpp_ng_bpf_t* obj);
int bpf_program_pool_events(struct perf_buffer* pb);

#ifdef __cplusplus
}
#endif

#endif // BPF_PROGRAM_UTILS_H
