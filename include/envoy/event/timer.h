#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

namespace Envoy {

class ScopeTrackedObject;

namespace Event {

/**
 * Callback invoked when a timer event fires.
 */
using TimerCb = std::function<void()>;

/**
 * An abstract timer event. Free the timer to unregister any pending timeouts. Must be freed before
 * the dispatcher is torn down.
 */
class Timer {
public:
  virtual ~Timer() = default;

  /**
   * Disable a pending timeout without destroying the underlying timer.
   */
  virtual void disableTimer() PURE;

  /**
   * Enable a pending timeout. If a timeout is already pending, it will be reset to the new timeout.
   *
   * @param ms supplies the duration of the alarm in milliseconds.
   * @param object supplies an optional scope for the duration of the alarm.
   */
  virtual void enableTimer(std::chrono::milliseconds ms,
                           const ScopeTrackedObject* object = nullptr) PURE;

  /**
   * Enable a pending high resolution timeout. If a timeout is already pending, it will be reset to
   * the new timeout.
   *
   * @param us supplies the duration of the alarm in microseconds.
   * @param object supplies an optional scope for the duration of the alarm.
   */
  virtual void enableHRTimer(std::chrono::microseconds us,
                             const ScopeTrackedObject* object = nullptr) PURE;
  /**
   * Return whether the timer is currently armed.
   */
  virtual bool enabled() PURE;
};

using TimerPtr = std::unique_ptr<Timer>;

} // namespace Event
} // namespace Envoy
