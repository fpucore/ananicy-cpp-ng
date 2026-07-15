#ifndef ANANICY_CPP_SINGLETON_PROCESS_HPP
#define ANANICY_CPP_SINGLETON_PROCESS_HPP

#include <cstdint>
#include <functional>
#include <string_view>
#include <sys/stat.h> // mode_t
#include <thread>

class SingletonProcess {
public:
  // Default constructor.
  SingletonProcess() noexcept = default;

  SingletonProcess(SingletonProcess &&) noexcept = default;
  SingletonProcess &operator=(SingletonProcess &&) noexcept = default;

  // Constructs with the name.
  // Used as semaphore name.
  SingletonProcess(std::string_view name) : m_filename(std::move(name)) {}

  // Destroys *this and indicates that the calling process is finished
  ~SingletonProcess();

  // Returns the name of the shared memory object.
  constexpr mode_t get_mode() const noexcept { return m_mode; }

  // Returns the name of the shared memory object.
  constexpr std::string_view get_name() const noexcept { return m_filename; }

  // Creates a shared memory object.
  bool try_create() noexcept;
  // Opens a shared memory object.
  bool try_open() noexcept;

  // Writes data to a shared memory object.
  bool try_write(std::string_view data) noexcept;

  // Closes a shared memory object on the current instance.
  // @returns false on error.
  bool close_shm() noexcept;

  // Starts listening on shared memory object.
  void start_read_queue(const std::function<void()> &func_callback) noexcept;

  // Erases a shared memory object from the system.
  // @returns false on error.
  static bool remove(std::string_view name) noexcept;

  // Deleted
  SingletonProcess(const SingletonProcess &) noexcept = delete;
  SingletonProcess &operator=(const SingletonProcess &) noexcept = delete;

private:
  std::int32_t          m_handle{-1};
  mode_t                m_mode{};
  std::string_view      m_filename{};
  std::jthread          m_worker_thread{};
  std::function<void()> m_reload_callback{};

  void read_queue(const std::stop_token &) noexcept;
};

#endif // ANANICY_CPP_SINGLETON_PROCESS_HPP
