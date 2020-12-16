#include "common/event/real_time_system.h"

#include <chrono>

#include "common/common/assert.h"
#include "common/event/timer_impl.h"

namespace Envoy {
namespace Event {
namespace {

class RealScheduler : public Scheduler {
public:
  RealScheduler(Scheduler& base_scheduler) : base_scheduler_(base_scheduler) {}
  TimerPtr createTimer(const TimerCb& cb) override { return base_scheduler_.createTimer(cb); };
  const ScopeTrackedObject* setTrackedObject(const ScopeTrackedObject* object) override { return base_scheduler_.setTrackedObject(object); }
  bool isThreadSafe() const override { return base_scheduler_.isThreadSafe(); }
  MonotonicTime approximateMonotonicTime() const override { return base_scheduler_.approximateMonotonicTime(); }

private:
  Scheduler& base_scheduler_;
};

} // namespace

SchedulerPtr RealTimeSystem::createScheduler(Scheduler& base_scheduler, CallbackScheduler&) {
  return std::make_unique<RealScheduler>(base_scheduler);
}

} // namespace Event
} // namespace Envoy
