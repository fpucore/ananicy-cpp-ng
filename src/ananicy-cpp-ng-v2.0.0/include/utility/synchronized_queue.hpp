#pragma once

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

#ifdef USE_EXPERIMENTAL_IMPL
#include "utility/atomic_queue/atomic_queue.h"

template <class T, class A = std::allocator<T>, bool MAXIMIZE_THROUGHPUT = true,
          bool TOTAL_ORDER = false, bool SPSC = false>
class synchronized_queue
    : public atomic_queue::AtomicQueueB2<T, A, MAXIMIZE_THROUGHPUT, TOTAL_ORDER,
                                         SPSC> {
  using Base =
      atomic_queue::AtomicQueueB2<T, A, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>;
  using Base::Base;

public:
  template <typename Rep, typename Period>
  std::optional<T> poll(std::chrono::duration<Rep, Period> timeout) noexcept {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_cond_not_empty.wait_for(lock, timeout,
                                  [this] { return !this->was_empty(); })) {
      if (this->was_empty()) {
        return {};
      }
      return this->pop();
    }
    return {};
  }

  [[nodiscard]] constexpr inline size_t size() const noexcept {
    return this->was_size();
  }

  synchronized_queue(const synchronized_queue &) = delete;
  synchronized_queue operator=(const synchronized_queue &) = delete;

private:
  std::mutex              m_mutex{};
  std::condition_variable m_cond_not_empty{};
};

#else

template <typename T> class synchronized_queue {
public:
  /*inline void push(const T &v) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(v);
    m_cond_not_empty.notify_one();
  }*/
  inline void push(std::convertible_to<T> auto &&v) noexcept {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queue.emplace(std::forward<decltype(v)>(v));
    m_cond_not_empty.notify_one();
  }

  inline T pop() noexcept {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond_not_empty.wait(lock, [this] { return !m_queue.empty(); });

    T front = m_queue.front();
    m_queue.pop();
    return front;
  }

  template <typename Rep, typename Period>
  std::optional<T> poll(std::chrono::duration<Rep, Period> timeout) noexcept {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_cond_not_empty.wait_for(lock, timeout,
                                  [this] { return !m_queue.empty(); })) {
      if (m_queue.empty()) {
        return {};
      }

      T front = m_queue.front();
      m_queue.pop();
      T value = front;

      return value;
    }
    return {};
  }

  template <typename Rep, typename Period>
  bool wait_for(std::chrono::duration<Rep, Period> timeout) noexcept {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_cond_not_empty.wait_for(lock, timeout,
                                     [this] { return !m_queue.empty(); });
  }

  [[nodiscard]] constexpr inline size_t size() const noexcept {
    return m_queue.size();
  }

  constexpr synchronized_queue() = default;
  constexpr synchronized_queue(const synchronized_queue &) = delete;
  constexpr synchronized_queue &operator=(const synchronized_queue &) = delete;

private:
  std::queue<T> m_queue;

  std::mutex              m_mutex;
  std::condition_variable m_cond_not_empty{};
};

#endif
