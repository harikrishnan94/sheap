#pragma once

#include <atomic>
#include <boost/interprocess/sync/spin/wait.hpp>
#include <cinttypes>
#include <mutex>

namespace sheap::detail {
class SpinLock {
public:
  SpinLock() = default;
  SpinLock(const SpinLock &) = delete;
  SpinLock(SpinLock &&) = delete;
  ~SpinLock() = default;

  bool try_lock() noexcept {
    std::int8_t unlocked = UNLOCKED;
    return m_state.compare_exchange_strong(unlocked, LOCKED);
  }

  void lock() noexcept {
    if (!try_lock()) {
      boost::interprocess::spin_wait swait;
      do {
        if (try_lock()) {
          break;
        } else {
          swait.yield();
        }
      } while (true);
    }
  }

  void unlock() noexcept { m_state = UNLOCKED; }

private:
  enum { UNLOCKED, LOCKED };
  std::atomic<std::int8_t> m_state = UNLOCKED;
};
} // namespace sheap::detail