#include "netlink_program_utils.h"

#include <linux/cn_proc.h>
#include <linux/connector.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <unistd.h>

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct ananicy_cpp_ng_netlink {
  int32_t nl_sock;
  struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
    struct nlmsghdr nl_hdr;
    struct __attribute__ ((__packed__)) {
      struct cn_msg     cn_msg;
      struct proc_event proc_ev;
    };
  } nl_cn_msg;
};

int32_t connect_netlink_socket() {
  int32_t nl_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
  if (nl_sock == -1) {
    perror("socket");
    return -1;
  }

  struct sockaddr_nl sa_nl = {
      .nl_family = AF_NETLINK,
      .nl_pid = getpid(),
      .nl_groups = CN_IDX_PROC,
  };

  int32_t rc = bind(nl_sock, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
  if (rc == -1) {
    perror("bind");
    close(nl_sock);
    return -1;
  }

  /* Timeout after some time, to avoid blocking forever */
  struct timeval tv = {
    .tv_sec = 0, .tv_usec = 500 * 1000
  };
  setsockopt(nl_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)(&tv), sizeof(tv));

  return nl_sock;
}

int32_t set_event_subscription(int32_t nl_sock, bool enable) {
  struct __attribute__ ((aligned(NLMSG_ALIGNTO))) nl_cn_msg {
    struct nlmsghdr nl_hdr;
    struct __attribute__ ((__packed__)) {
      struct cn_msg         cn_msg;
      enum proc_cn_mcast_op cn_mcast;
    };
  };

  struct nl_cn_msg netlink_cn_msg = {};
  netlink_cn_msg.nl_hdr.nlmsg_len = sizeof(struct nl_cn_msg);
  netlink_cn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;
  netlink_cn_msg.nl_hdr.nlmsg_pid = getpid();

  netlink_cn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);
  netlink_cn_msg.cn_msg.id.idx = CN_IDX_PROC;
  netlink_cn_msg.cn_msg.id.val = CN_VAL_PROC;

  netlink_cn_msg.cn_mcast = enable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

  return send(nl_sock, &netlink_cn_msg, sizeof(struct nl_cn_msg), 0);
}

ananicy_cpp_ng_netlink_t* initialize_netlink_program() {
  ananicy_cpp_ng_netlink_t* obj = NULL;
  obj = (ananicy_cpp_ng_netlink_t*)calloc(1, sizeof(struct ananicy_cpp_ng_netlink));
  if (!obj) {
    perror("initialize_netlink_program");
    return NULL;
  }
  obj->nl_sock = connect_netlink_socket();

  const int32_t listen_status = set_event_subscription(obj->nl_sock, true);
  if (listen_status == 0) {
    close(obj->nl_sock);
    free(obj);
    return NULL;
  }
  return obj;
}

void destroy_netlink_program(ananicy_cpp_ng_netlink_t* obj) {
  if (!obj)
    return;

  set_event_subscription(obj->nl_sock, false);
  close(obj->nl_sock);
  free(obj);
}

uint64_t netlink_program_get_timestamp(ananicy_cpp_ng_netlink_t* obj) {
  return obj->nl_cn_msg.proc_ev.timestamp_ns;
}

pid_t netlink_program_get_pid(ananicy_cpp_ng_netlink_t* obj) {
  pid_t pid = 0;

  struct proc_event proc_ev = obj->nl_cn_msg.proc_ev;
  switch (proc_ev.what) {
  case PROC_EVENT_FORK:
    pid = proc_ev.event_data.fork.child_pid;
    break;
  case PROC_EVENT_EXEC:
    pid = proc_ev.event_data.exec.process_pid;
    break;
  case PROC_EVENT_COMM:
    pid = proc_ev.event_data.comm.process_pid;
    break;
  default:
    break; /* we don't care */
  }
  return pid;
}

ssize_t netlink_program_pool_message(ananicy_cpp_ng_netlink_t* obj) {
  return recv(obj->nl_sock, &obj->nl_cn_msg, sizeof(obj->nl_cn_msg), 0);
}
