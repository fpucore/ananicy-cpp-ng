// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// Copyright (c) 2019 Facebook
//
// Based on runqslower(8) from BCC by Ivan Babrou.
// 11-Feb-2020   Andrii Nakryiko   Created this.
#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bpf_program_utils.h"

#define TASK_COMM_LEN 16

struct event {
  pid_t pid;
  pid_t prev_pid;
  __u64 delta_us;
  char  task[TASK_COMM_LEN];
};

static volatile sig_atomic_t exiting = 0;

struct env {
  __u64 min_us;
  bool  verbose;
} env = {
    .min_us = 10000UL,
};

const char argp_program_doc[] =
    "Trace high run queue latency.\n"
    "\n"
    "USAGE: runqslower_c [--help] [min_us]\n"
    "\n"
    "EXAMPLES:\n"
    "    runqslower_c         # trace latency higher than 10000 us (default)\n"
    "    runqslower_c 1000    # trace latency higher than 1000 us\n";

static const struct argp_option opts[] = {
    {"verbose", 'v', NULL, 0, "Verbose debug output"},
    {NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help"},
    {},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state) {
  static int pos_args;
  long long  min_us;

  switch (key) {
  case 'h':
    argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
    break;
  case 'v':
    env.verbose = true;
    break;
  case ARGP_KEY_ARG:
    if (pos_args++) {
      fprintf(stderr, "Unrecognized positional argument: %s\n", arg);
      argp_usage(state);
    }
    errno = 0;
    min_us = strtoll(arg, NULL, 10);
    if (errno || min_us <= 0) {
      fprintf(stderr, "Invalid delay (in us): %s\n", arg);
      argp_usage(state);
    }
    env.min_us = min_us;
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static void sig_int(int signo) { exiting = 1; }

void handle_event(void *ctx, int32_t cpu, void *data, uint32_t data_sz) {
  struct event *e = data;

  if (e->pid == 0 || e->pid == e->prev_pid) {
    return;
  }

  printf("(%16s) %7d %14llu\n", e->task, e->pid, e->delta_us);
}

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt) {
  fprintf(stderr, "Lost %llu events on CPU %d!\n", lost_cnt, cpu);
}

int main(int argc, char **argv) {
  static const struct argp argp = {
      .options = opts,
      .parser = parse_arg,
      .doc = argp_program_doc,
  };
  int32_t err_code = argp_parse(&argp, argc, argv, 0, NULL, NULL);
  if (err_code)
    return err_code;

  ananicy_cpp_ng_bpf_t *obj = initialize_bpf_program(env.min_us, env.verbose);
  if (!obj) {
    fprintf(stderr, "failed to initialize BPF program\n");
    return 1;
  }

  printf("Tracing run queue latency higher than %llu us\n", env.min_us);
  printf("%18s %7s %14s\n", "COMM", "TID", "LAT(us)");

  struct perf_buffer *pb = NULL;
  pb = bpf_program_init_events(obj);
  if (!pb) {
    err_code = -errno;
    fprintf(stderr, "failed to open perf buffer: %d\n", err_code);
    destroy_bpf_program(obj, pb);
    return err_code;
  }

  if (signal(SIGINT, sig_int) == SIG_ERR) {
    fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
    destroy_bpf_program(obj, pb);
    return 1;
  }

  while (!exiting) {
    err_code = perf_buffer__poll(pb, 100);
    if (err_code < 0 && err_code != -EINTR) {
      fprintf(stderr, "error polling perf buffer: %s\n", strerror(-err_code));
      destroy_bpf_program(obj, pb);
      return err_code;
    }
    /* reset err to return 0 if exiting */
    err_code = 0;
  }
  return err_code;
}
