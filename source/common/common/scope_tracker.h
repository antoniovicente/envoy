#pragma once

#include "envoy/common/scope_tracker.h"
#include "envoy/event/scheduler.h"

namespace Envoy {

// A small class for tracking the scope of the object which is currently having
// work done in this thread.
//
// When created, it sets the tracked object in the scheduler, and when destroyed it points the
// scheduler at the previously tracked object.
class ScopeTrackerScopeState {
public:
  ScopeTrackerScopeState(const ScopeTrackedObject* object, Event::Scheduler& scheduler)
      : scheduler_(scheduler) {
    latched_object_ = scheduler_.setTrackedObject(object);
  }

  ~ScopeTrackerScopeState() { scheduler_.setTrackedObject(latched_object_); }

private:
  const ScopeTrackedObject* latched_object_;
  Event::Scheduler& scheduler_;
};

} // namespace Envoy
