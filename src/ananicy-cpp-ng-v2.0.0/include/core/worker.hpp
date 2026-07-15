//
// Created by aviallon on 20/04/2021.
//

#ifndef ANANICY_CPP_WORKER_HPP
#define ANANICY_CPP_WORKER_HPP

#include "config.hpp"
#include "process.hpp"
#include "rules.hpp"
#include "utility/synchronized_queue.hpp"
#include "utility/utils.hpp"

#include <thread>

class Worker {
public:
  Worker(Rules *rules, Config *config,
         synchronized_queue<Process> *process_queue);
  void start();
  void stop() {
    worker_thread.request_stop();
    worker_thread.join();
  }

  [[nodiscard]] unsigned processed_processes() const { return processed_count; }

private:
  Rules                       *rules;
  Config                      *config;
  std::jthread                 worker_thread;
  synchronized_queue<Process> *process_queue;
  std::atomic<unsigned>        processed_count;

  void work(const std::stop_token &stop_token);
};

#endif // ANANICY_CPP_WORKER_HPP
