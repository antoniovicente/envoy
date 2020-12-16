#pragma once

#include "envoy/event/time_system.h"

#include "common/common/utility.h"

namespace Envoy {
namespace Event {

/**
 * Real-world time implementation of TimeSystem.
 */
class RealTimeSystem : public TimeSystem {
public:
  // TimeSystem
  SchedulerPtr createScheduler(Scheduler&, CallbackScheduler&) override;

  // TimeSource
  SystemTime systemTime() override { return time_source_.systemTime(); }
  MonotonicTime monotonicTime() override { return time_source_.monotonicTime(); }

private:
  RealTimeSource time_source_;
};

} // namespace Event
} // namespace Envoy
