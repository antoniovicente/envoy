#pragma once

#include <memory>

#include "envoy/common/pure.h"
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
};

using SchedulerPtr = std::unique_ptr<Scheduler>;

} // namespace Event
} // namespace Envoy
