#pragma once

#include <windows.h>

// UI-thread timer wrapper. SetTimer callbacks are dispatched by the window
// message loop, so GUI/GDI state is never touched from a worker thread.
class CTimer {
public:
  CTimer() = default;
  ~CTimer() { Stop(); }

  CTimer(const CTimer &) = delete;
  CTimer &operator=(const CTimer &) = delete;

  bool Start(unsigned int interval, bool immediately = false,
             bool once = false) {
    Stop();
    interval_ = interval == 0 ? 1 : interval;
    once_ = once;
    id_ = SetTimer(nullptr, 0, interval_, TimerProc);
    if (!id_) {
      return false;
    }
    if (!Register(id_, this)) {
      return false;
    }
    if (immediately && OnTimedEvent) {
      OnTimedEvent();
      if (once_) {
        Stop();
      }
    }
    return true;
  }

  void Stop() {
    if (!id_) {
      return;
    }
    const UINT_PTR oldId = id_;
    id_ = 0;
    Unregister(oldId);
    KillTimer(nullptr, oldId);
  }

  [[nodiscard]] bool Enabled() const noexcept { return id_ != 0; }

  void (*OnTimedEvent)() = nullptr;

private:
  struct Entry {
    UINT_PTR id;
    CTimer *timer;
  };

  // Only the message-loop thread accesses this small registry.
  static inline Entry entries_[8]{};

  static bool Register(UINT_PTR id, CTimer *timer) {
    for (auto &entry : entries_) {
      if (!entry.timer) {
        entry = {id, timer};
        return true;
      }
    }
    KillTimer(nullptr, id);
    timer->id_ = 0;
    return false;
  }

  static void Unregister(UINT_PTR id) {
    for (auto &entry : entries_) {
      if (entry.id == id) {
        entry = {};
        return;
      }
    }
  }

  static CTimer *Find(UINT_PTR id) {
    for (auto &entry : entries_) {
      if (entry.id == id) {
        return entry.timer;
      }
    }
    return nullptr;
  }

  static void CALLBACK TimerProc(HWND, UINT, UINT_PTR id, DWORD) {
    CTimer *timer = Find(id);
    if (!timer || !timer->OnTimedEvent) {
      return;
    }
    const bool once = timer->once_;
    timer->OnTimedEvent();
    if (once && timer->id_ == id) {
      timer->Stop();
    }
  }

  UINT_PTR id_ = 0;
  unsigned int interval_ = 0;
  bool once_ = false;
};
