#ifndef ANANICY_CPP_SYSCALLS_H
#define ANANICY_CPP_SYSCALLS_H

#include <sys/resource.h>
#include <sys/syscall.h>
#include <cerrno>
#include <unistd.h>

#include <cstdio>
#include <cstdint>

/**
 * ioprio_(get|set)
 */

#define IOPRIO_CLASS_SHIFT (13)
#define IOPRIO_PRIO_MASK   ((1UL << IOPRIO_CLASS_SHIFT) - 1)

#define IOPRIO_PRIO_CLASS(mask) ((mask) >> IOPRIO_CLASS_SHIFT)
#define IOPRIO_PRIO_DATA(mask)  ((mask)&IOPRIO_PRIO_MASK)
#define IOPRIO_PRIO_VALUE(class, data)                                         \
  (((class) << IOPRIO_CLASS_SHIFT) | (data))

#define ioprio_valid(mask) (IOPRIO_PRIO_CLASS((mask)) != IOPRIO_CLASS_NONE)

enum {
  IOPRIO_CLASS_NONE = 0,
  IOPRIO_CLASS_RT = 1,
  IOPRIO_CLASS_BE = 2,
  IOPRIO_CLASS_IDLE = 3,
};

/*
 * 8 best effort priority levels are supported
 */
#define IOPRIO_BE_NR (8)

enum {
  IOPRIO_WHO_PROCESS = 1,
  IOPRIO_WHO_PGRP,
  IOPRIO_WHO_USER,
};

/*
 * Fallback BE priority
 */
#define IOPRIO_NORM (4)

static int ioprio_set(__priority_which_t _which, id_t _who, int _prio) {
  return static_cast<int>(syscall(SYS_ioprio_set, _which, _who, _prio));
}

static int ioprio_get(__priority_which_t _which, id_t _who) {
  return static_cast<int>(syscall(SYS_ioprio_get, _which, _who));
}

/**
 * sched_(set|get)attr
 */

// #include <linux/sched/types.h>
#ifndef SCHED_FLAG_LATENCY_NICE
#define SCHED_FLAG_LATENCY_NICE         0x80
#endif
#ifndef SCHED_FLAG_KEEP_PARAMS
#define SCHED_FLAG_KEEP_PARAMS          0x10
#endif
#ifndef SCHED_FLAG_KEEP_POLICY
#define SCHED_FLAG_KEEP_POLICY          0x08
#endif

struct [[gnu::packed]] ananicy_sched_attr {
  uint32_t size;

  uint32_t sched_policy; // SCHED_(FIFO,RR,DEADLINE,OTHER,BATCH,IDLE, etc.)
  uint64_t sched_flags;

  int32_t sched_nice; // For non-realtime policies

  uint32_t sched_priority; // For realtime policies

  /**
   * SCHED_DEADLINE specific stuff
   */
  uint64_t sched_runtime;
  uint64_t sched_deadline;
  uint64_t sched_period;

  /* Utilization hints */
  uint32_t sched_util_min;
  uint32_t sched_util_max;

  /* latency requirement hints */
  int32_t sched_latency_nice;
};

static int ananicy_sched_setattr(pid_t pid, const struct ananicy_sched_attr *attr,
                         unsigned int flags) {
  return static_cast<int>(syscall(__NR_sched_setattr, pid, attr, flags));
}

static int ananicy_sched_getattr(pid_t pid, struct ananicy_sched_attr *attr, unsigned int size,
                         unsigned int flags) {
  return static_cast<int>(syscall(__NR_sched_getattr, pid, attr, size, flags));
}

static int get_latnice(pid_t pid) {
  // pid==0 refers to calling thread
  struct ananicy_sched_attr attr = { .size = sizeof(struct ananicy_sched_attr) };
  if (ananicy_sched_getattr(pid, &attr, sizeof(attr), 0) < 0) {
      std::perror("ananicy_sched_getattr");
  }
  return attr.sched_latency_nice; // defaults to 0
}

static int set_latnice(pid_t pid, int latency_nice) {
  // pid==0 refers to calling thread
  struct ananicy_sched_attr attr = {
    .size = sizeof(struct ananicy_sched_attr),
    .sched_flags = SCHED_FLAG_LATENCY_NICE | SCHED_FLAG_KEEP_PARAMS,
    .sched_latency_nice = latency_nice,
  };
  const int err = ananicy_sched_setattr(pid, &attr, 0);
  if (err < 0) {
    // ananicy_sched_setattr failed
    if (errno == EINVAL) {
      // Don't print strerror for EINVAL
      errno = 0;
      return err;
    }
    // Other error occurred
    std::perror("ananicy_sched_setattr");
  }
  return err;
}

#endif // ANANICY_CPP_SYSCALLS_H
