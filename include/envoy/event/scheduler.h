#pragma once

#include <memory>

#include "envoy/common/pure.h"
#include "envoy/common/scope_tracker.h"
#include "envoy/common/time.h"
#include "envoy/event/timer.h"

namespace Envoy {
namespace Event {

/**
 * Minimal interface to an event loop used to create low-level primitives. See dispatcher.h for the
 * full event loop interface.
 */
class Scheduler {
public:
  virtual ~Scheduler() = default;

  /**
   * Allocates a timer. @see Timer for docs on how to use the timer.
   * @param cb supplies the callback to invoke when the timer fires.
   */
  virtual TimerPtr createTimer(const TimerCb& cb) PURE;

  /**
   * Sets a tracked object, which is currently operating in this Dispatcher.
   * This should be cleared with another call to setTrackedObject() when the object is done doing
   * work. Calling setTrackedObject(nullptr) results in no object being tracked.
   *
   * This is optimized for performance, to avoid allocation where we do scoped object tracking.
   *
   * @return The previously tracked object or nullptr if there was none.
   */
  virtual const ScopeTrackedObject* setTrackedObject(const ScopeTrackedObject* object) PURE;

  /**
   * Validates that an operation is thread-safe with respect to this dispatcher; i.e. that the
   * current thread of execution is on the same thread upon which the dispatcher loop is running.
   */
  virtual bool isThreadSafe() const PURE;

  /**
   * Returns a recently cached MonotonicTime value.
   */
  virtual MonotonicTime approximateMonotonicTime() const PURE;
};

using SchedulerPtr = std::unique_ptr<Scheduler>;

} // namespace Event
} // namespace Envoy
