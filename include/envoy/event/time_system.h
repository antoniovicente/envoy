#pragma once

#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "envoy/event/schedulable_cb.h"
#include "envoy/event/scheduler.h"

namespace Envoy {
namespace Event {

/**
 * Interface providing a mechanism to measure time and set timers that run callbacks
 * when the timer fires.
 */
class TimeSystem : public TimeSource {
public:
  ~TimeSystem() override = default;

  using Duration = MonotonicTime::duration;
  using Nanoseconds = std::chrono::nanoseconds;
  using Microseconds = std::chrono::microseconds;
  using Milliseconds = std::chrono::milliseconds;
  using Seconds = std::chrono::seconds;

  /**
   * Creates a timer factory. This indirection enables thread-local timer-queue management,
   * so servers can have a separate timer-factory in each thread.
   */
  virtual SchedulerPtr createScheduler(Scheduler& base_scheduler,
                                       CallbackScheduler& cb_scheduler) PURE;
};

} // namespace Event
} // namespace Envoy
