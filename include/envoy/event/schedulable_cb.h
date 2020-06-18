#pragma once

#include <functional>
#include <memory>

#include "envoy/common/pure.h"

namespace Envoy {
namespace Event {

/**
 * Callback wrapper that allows direct scheduling of callbacks in the event loop.
 */
class SchedulableCallback {
public:
  virtual ~SchedulableCallback() = default;

  /**
   * Schedule the callback.
   * @param type specifies wherever to run as part of the current or next event loop iteration.
   */
  enum class ScheduleType {
    // Schedule the callback so it executes as part of the current event loop iteration. Callback
    // will execute before polling for fd events.
    CurrentLoopIteration,
    // Schedule the callback so it executes as part of the next event loop iteration. Callback will
    // execute after polling for and processing fd events.
    NextLoopIteration,
  };
  virtual void scheduleCallback(ScheduleType type) PURE;
  /**
   * Cancel pending execution of the callback.
   */
  virtual void cancel() PURE;

  /**
   * Return true whether the SchedulableCallback is scheduled for execution.
   */
  virtual bool enabled() PURE;
};

using SchedulableCallbackPtr = std::unique_ptr<SchedulableCallback>;

/**
 * SchedulableCallback factory.
 */
class CallbackScheduler {
public:
  virtual ~CallbackScheduler() = default;

  /**
   * Create a schedulable callback.
   */
  virtual SchedulableCallbackPtr createSchedulableCallback(const std::function<void()>& cb) PURE;
};

} // namespace Event
} // namespace Envoy
