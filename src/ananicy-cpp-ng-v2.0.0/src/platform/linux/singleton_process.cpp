#include <cstring>
#include "utility/singleton_process.hpp"

#include <cerrno>
#include <chrono>
#include <fcntl.h> /* For O_* constants */
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>

#include <string_view> // for string_view

#include <fmt/format.h>

#include <spdlog/spdlog.h>

using hash_t = std::uint64_t;

static constexpr auto SHARED_MEMORY_ADDR_SIZE = 1024;

// Hardcoded values for the prime and the basis to calculate hash values.
static constexpr hash_t prime = 0x100000001B3ull;
static constexpr hash_t basis = 0xCBF29CE484222325ull;

consteval hash_t hash_compile_time(const char *str, hash_t last_value = basis) {
  return (*str) ? hash_compile_time(
                      str + 1, (static_cast<hash_t>(*str) ^ last_value) * prime)
                : last_value;
}

// The key for the shared memory segment
static constexpr key_t shm_key =
    static_cast<key_t>(hash_compile_time("AnanicyCppMutex"));

SingletonProcess::~SingletonProcess() {
  if (m_handle != -1) {
    this->close_shm();
    m_handle = -1;
  }
}

bool SingletonProcess::try_create() noexcept {
  const mode_t unix_perm = 0644;
  m_mode = O_RDWR | O_CREAT | O_EXCL;

  // Try to create shared memory
  m_handle = shm_open(m_filename.data(), m_mode, unix_perm);
  if (m_handle == -1) {
    if (errno != EEXIST)
      spdlog::error(fmt::format("Could not create: {}",
                                std::string_view{std::strerror(errno)}));
    return false;
  }
  // If successful change real permissions
  ::fchmod(m_handle, unix_perm);
  return true;
}

bool SingletonProcess::try_open() noexcept {
  if (m_handle != -1) {
    spdlog::error("Already opened!");
    return false;
  }

  const mode_t unix_perm = 0644;
  m_mode = O_RDONLY;

  // Try to open shared memory
  m_handle = shm_open(m_filename.data(), m_mode, unix_perm);
  if (m_handle == -1) {
    spdlog::error(fmt::format("Could not open: {}", std::strerror(errno)));
    return false;
  }
  return true;
}

bool SingletonProcess::try_write(std::string_view message) noexcept {
  // Get the shared memory segment
  int shmid = shmget(shm_key, SHARED_MEMORY_ADDR_SIZE, 0666);
  if (shmid < 0) {
    spdlog::error("Failed to get shared memory segment");
    return false;
  }

  // Attach the shared memory segment to the process's address space
  void *shms_address = shmat(shmid, nullptr, 0);
  if (shms_address == (void *)-1) {
    spdlog::error("Failed to attach shared memory segment");
    return false;
  }

  // Write the message to the shared memory segment
  std::copy_n(message.begin(), message.size(),
              static_cast<char *>(shms_address));
  fmt::print("Reload signal has been sent\n");

  // Detach the shared memory segment from the process's address space
  shmdt(shms_address);
  return true;
}

bool SingletonProcess::close_shm() noexcept {
  if (::close(m_handle) != 0) {
    spdlog::error("Could not close: {}", std::strerror(errno));
    return false;
  }
  m_handle = -1;
  return true;
}

bool SingletonProcess::remove(std::string_view name) noexcept {
  return 0 == shm_unlink(name.data());
}

void SingletonProcess::read_queue(const std::stop_token &stop_token) noexcept {
  // Open the shared memory object for reading
  int shmid = shmget(shm_key, SHARED_MEMORY_ADDR_SIZE, IPC_CREAT | 0666);
  if (shmid < 0) {
    spdlog::error("Failed to create shared memory segment");
    return;
  }

  // Attach the shared memory segment to the process's address space
  void *shms_address = shmat(shmid, nullptr, 0);
  if (shms_address == (void *)-1) {
    spdlog::error("Failed to attach shared memory segment");
    return;
  }

  while (!stop_token.stop_requested()) {
    char *buffer = static_cast<char *>(shms_address);
    if ((buffer == nullptr) || (buffer[0] == '\0')) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::string_view message{buffer};
    if (message == "RELOAD") {
      // Reload the configuration
      spdlog::info("Reloading configuration...");
      this->m_reload_callback();
    }

    std::memset(buffer, '\0', SHARED_MEMORY_ADDR_SIZE);
  }
  // Detach the shared memory segment from the process's address space
  shmdt(shms_address);
}

void SingletonProcess::start_read_queue(
    const std::function<void()> &func_callback) noexcept {
  m_reload_callback = func_callback;
  m_worker_thread =
      std::jthread(std::bind_front(&SingletonProcess::read_queue, this));
}
