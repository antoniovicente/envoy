#include "common/common/thread.h"
#include "common/event/libevent.h"
#include "common/event/libevent_scheduler.h"
#include "common/event/timer_impl.h"

#include "test/mocks/common.h"
#include "test/mocks/event/mocks.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/test_runtime.h"
#include "test/test_common/utility.h"

#include "event2/event.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Event {
namespace Test {
namespace {

class SimulatedTimeSystemTest : public testing::TestWithParam<bool> {
protected:
  SimulatedTimeSystemTest()
      : scheduler_(time_system_.createScheduler(base_scheduler_, base_scheduler_)),
        start_monotonic_time_(time_system_.monotonicTime()),
        start_system_time_(time_system_.systemTime()) {
    Runtime::LoaderSingleton::getExisting()->mergeValues(
        {{"envoy.reloadable_features.activate_timers_next_event_loop",
          activateTimersNextEventLoop() ? "true" : "false"}});
  }

  bool activateTimersNextEventLoop() { return GetParam(); }

  void trackPrepareCalls() {
    base_scheduler_.registerOnPrepareCallback([this]() { output_.append(1, 'p'); });
  }

  void addTask(int64_t delay_ms, char marker, bool expect_monotonic = true) {
    addCustomTask(
        delay_ms, marker, []() {}, expect_monotonic);
  }

  void addCustomTask(int64_t delay_ms, char marker, std::function<void()> cb,
                     bool expect_monotonic = true) {
    std::chrono::milliseconds delay(delay_ms);
    TimerPtr timer = scheduler_->createTimer(
        [this, marker, delay, cb, expect_monotonic]() {
          output_.append(1, marker);
          if (expect_monotonic) {
            EXPECT_GE(time_system_.monotonicTime(), start_monotonic_time_ + delay);
          }
          cb();
        },
        dispatcher_);
    timer->enableTimer(delay);
    timers_.push_back(std::move(timer));
  }

  void advanceMsAndLoop(int64_t delay_ms) {
    time_system_.advanceTimeAsync(std::chrono::milliseconds(delay_ms));
    base_scheduler_.run(Dispatcher::RunType::NonBlock);
  }

  void advanceSystemMsAndLoop(int64_t delay_ms) {
    time_system_.setSystemTime(time_system_.systemTime() + std::chrono::milliseconds(delay_ms));
    base_scheduler_.run(Dispatcher::RunType::NonBlock);
  }

  TestScopedRuntime scoped_runtime_;
  Event::MockDispatcher dispatcher_;
  LibeventScheduler base_scheduler_;
  SimulatedTimeSystem time_system_;
  SchedulerPtr scheduler_;
  std::string output_;
  std::vector<TimerPtr> timers_;
  MonotonicTime start_monotonic_time_;
  SystemTime start_system_time_;
};

INSTANTIATE_TEST_SUITE_P(DelayTimerActivation, SimulatedTimeSystemTest, testing::Bool());

TEST_P(SimulatedTimeSystemTest, AdvanceTimeAsync) {
  EXPECT_EQ(start_monotonic_time_, time_system_.monotonicTime());
  EXPECT_EQ(start_system_time_, time_system_.systemTime());
  advanceMsAndLoop(5);
  EXPECT_EQ(start_monotonic_time_ + std::chrono::milliseconds(5), time_system_.monotonicTime());
  EXPECT_EQ(start_system_time_ + std::chrono::milliseconds(5), time_system_.systemTime());
}

TEST_P(SimulatedTimeSystemTest, TimerTotalOrdering) {
  trackPrepareCalls();

  addTask(0, '0');
  addTask(1, '1');
  addTask(2, '2');
  EXPECT_EQ(3, timers_.size());

  advanceMsAndLoop(5);

  // Verify order.
  EXPECT_EQ("p012", output_);
}

TEST_P(SimulatedTimeSystemTest, TimerPartialOrdering) {
  trackPrepareCalls();

  std::set<std::string> outputs;
  for (int i = 0; i < 100; ++i) {
    addTask(0, '0');
    addTask(1, '1');
    addTask(1, '2');
    addTask(3, '3');
    EXPECT_EQ(4, timers_.size());

    advanceMsAndLoop(5);

    outputs.insert(output_);

    // Cleanup before the next iteration.
    output_.clear();
    timers_.clear();
  }

  // Execution order of timers 1 and 2 is non-deterministic because the two timers were scheduled
  // for the same time. Verify that both orderings were observed.
  EXPECT_THAT(outputs, testing::ElementsAre("p0123", "p0213"));
}

TEST_P(SimulatedTimeSystemTest, TimerPartialOrdering2) {
  trackPrepareCalls();

  std::set<std::string> outputs;
  for (int i = 0; i < 100; ++i) {
    addTask(0, '0');
    addTask(15, '1');
    advanceMsAndLoop(10);

    // Timer 1 has 5ms remaining, so timer 2 ends up scheduled at the same monotonic time as 1.
    addTask(5, '2');
    addTask(6, '3');
    advanceMsAndLoop(10);

    outputs.insert(output_);

    // Cleanup before the next iteration.
    output_.clear();
    timers_.clear();
  }

  // Execution order of timers 1 and 2 is non-deterministic because the two timers were scheduled
  // for the same time. Verify that both orderings were observed.
  EXPECT_THAT(outputs, testing::ElementsAre("p0p123", "p0p213"));
}

// Timers that are scheduled to execute and but are disabled first do not trigger.
TEST_P(SimulatedTimeSystemTest, TimerOrderAndDisableTimer) {
  trackPrepareCalls();

  // Create 3 timers. The first timer should disable the second, so it doesn't trigger.
  addCustomTask(0, '0', [this]() { timers_[1]->disableTimer(); });
  addTask(1, '1');
  addTask(2, '2');
  EXPECT_EQ(3, timers_.size());

  // Expect timers to execute in order since the timers are scheduled at have different times and
  // that timer 1 does not execute because it was disabled as part of 0's execution.
  advanceMsAndLoop(5);
  // Verify that timer 1 was skipped.
  EXPECT_EQ("p02", output_);
}

// Capture behavior of timers which are rescheduled without being disabled first.
TEST_P(SimulatedTimeSystemTest, TimerOrderAndRescheduleTimer) {
  trackPrepareCalls();

  // Reschedule timers 1, 2 and 4 without disabling first.
  addCustomTask(0, '0', [this]() {
    timers_[1]->enableTimer(std::chrono::milliseconds(0));
    timers_[2]->enableTimer(std::chrono::milliseconds(100));
    timers_[4]->enableTimer(std::chrono::milliseconds(0));
  });
  addTask(1, '1');
  addTask(2, '2');
  addTask(3, '3');
  addTask(10000, '4', false);
  EXPECT_EQ(5, timers_.size());

  // Rescheduling timers that are already scheduled to run in the current event loop iteration has
  // no effect if the time delta is 0. Expect timers 0, 1 and 3 to execute in the original order.
  // Timer 4 runs as part of the first wakeup since its new schedule time has a delta of 0. Timer 2
  // is delayed since it is rescheduled with a non-zero delta.
  advanceMsAndLoop(5);
  if (activateTimersNextEventLoop()) {
#ifdef WIN32
    // The event loop runs for a single iteration in NonBlock mode on Windows. Force it to run again
    // to pick up next iteration callbacks.
    advanceMsAndLoop(0);
#endif
    EXPECT_EQ("p013p4", output_);
  } else {
    EXPECT_EQ("p0134", output_);
  }

  advanceMsAndLoop(100);
  if (activateTimersNextEventLoop()) {
    EXPECT_EQ("p013p4p2", output_);
  } else {
    EXPECT_EQ("p0134p2", output_);
  }
}

// Disable and re-enable timers that is already pending execution and verify that execution is
// delayed.
TEST_P(SimulatedTimeSystemTest, TimerOrderDisableAndRescheduleTimer) {
  trackPrepareCalls();

  // Disable and reschedule timers 1, 2 and 4 when timer 0 triggers.
  addCustomTask(0, '0', [this]() {
    timers_[1]->disableTimer();
    timers_[1]->enableTimer(std::chrono::milliseconds(0));
    timers_[2]->disableTimer();
    timers_[2]->enableTimer(std::chrono::milliseconds(100));
    timers_[4]->disableTimer();
    timers_[4]->enableTimer(std::chrono::milliseconds(0));
  });
  addTask(1, '1');
  addTask(2, '2');
  addTask(3, '3');
  addTask(10000, '4', false);
  EXPECT_EQ(5, timers_.size());

  // timer 0 is expected to run first and reschedule timers 1 and 2. Timer 3 should fire before
  // timer 1 since timer 3's registration is unaffected. timer 1 runs in the same iteration
  // because it is scheduled with zero delay. Timer 2 executes in a later iteration because it is
  // re-enabled with a non-zero timeout.
  advanceMsAndLoop(5);
  if (activateTimersNextEventLoop()) {
#ifdef WIN32
    // The event loop runs for a single iteration in NonBlock mode on Windows. Force it to run again
    // to pick up next iteration callbacks.
    advanceMsAndLoop(0);
#endif
    EXPECT_EQ("p03p14", output_);
  } else {
    EXPECT_EQ("p0314", output_);
  }

  advanceMsAndLoop(100);
  if (activateTimersNextEventLoop()) {
    EXPECT_EQ("p03p14p2", output_);
  } else {
    EXPECT_EQ("p0314p2", output_);
  }
}

TEST_P(SimulatedTimeSystemTest, AdvanceTimeWait) {
  EXPECT_EQ(start_monotonic_time_, time_system_.monotonicTime());
  EXPECT_EQ(start_system_time_, time_system_.systemTime());

  addTask(4, 'Z');
  addTask(2, 'X');
  addTask(3, 'Y');
  addTask(6, 'A'); // This timer will never be run, so "A" will not be appended.
  std::atomic<bool> done(false);
  auto thread = Thread::threadFactoryForTest().createThread([this, &done]() {
    while (!done) {
      base_scheduler_.run(Dispatcher::RunType::Block);
    }
  });
  time_system_.advanceTimeWait(std::chrono::milliseconds(5));
  EXPECT_EQ("XYZ", output_);
  done = true;
  thread->join();
  EXPECT_EQ(start_monotonic_time_ + std::chrono::milliseconds(5), time_system_.monotonicTime());
  EXPECT_EQ(start_system_time_ + std::chrono::milliseconds(5), time_system_.systemTime());
}

TEST_P(SimulatedTimeSystemTest, WaitFor) {
  EXPECT_EQ(start_monotonic_time_, time_system_.monotonicTime());
  EXPECT_EQ(start_system_time_, time_system_.systemTime());

  // Run an event loop in the background to activate timers.
  std::atomic<bool> done(false);
  auto thread = Thread::threadFactoryForTest().createThread([this, &done]() {
    while (!done) {
      base_scheduler_.run(Dispatcher::RunType::Block);
    }
  });
  Thread::MutexBasicLockable mutex;
  Thread::CondVar condvar;
  TimerPtr timer = scheduler_->createTimer(
      [&condvar, &mutex, &done]() {
        Thread::LockGuard lock(mutex);
        done = true;
        condvar.notifyOne();
      },
      dispatcher_);
  timer->enableTimer(std::chrono::seconds(60));

  // Wait 50 simulated seconds of simulated time, which won't be enough to
  // activate the alarm. We'll get a fast automatic timeout in waitFor because
  // there are no pending timers.
  {
    Thread::LockGuard lock(mutex);
    EXPECT_EQ(Thread::CondVar::WaitStatus::Timeout,
              time_system_.waitFor(mutex, condvar, std::chrono::seconds(50)));
  }
  EXPECT_FALSE(done);
  EXPECT_EQ(MonotonicTime(std::chrono::seconds(50)), time_system_.monotonicTime());

  // Waiting another 20 simulated seconds will activate the alarm after 10,
  // and the event-loop thread will call the corresponding callback quickly.
  {
    Thread::LockGuard lock(mutex);
    // We don't check for the return value of waitFor() as it can spuriously
    // return timeout even if the condition is satisfied before entering into
    // the waitFor().
    //
    // TODO(jmarantz): just drop the return value in the API.
    time_system_.waitFor(mutex, condvar, std::chrono::seconds(10));
  }
  EXPECT_TRUE(done);
  EXPECT_EQ(MonotonicTime(std::chrono::seconds(60)), time_system_.monotonicTime());

  // Waiting a third time, with no pending timeouts, will just sleep out for
  // the max duration and return a timeout.
  done = false;
  {
    Thread::LockGuard lock(mutex);
    EXPECT_EQ(Thread::CondVar::WaitStatus::Timeout,
              time_system_.waitFor(mutex, condvar, std::chrono::seconds(20)));
  }
  EXPECT_FALSE(done);
  EXPECT_EQ(MonotonicTime(std::chrono::seconds(80)), time_system_.monotonicTime());

  thread->join();
}

TEST_P(SimulatedTimeSystemTest, Monotonic) {
  // Setting time forward works.
  time_system_.setMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(5));
  EXPECT_EQ(start_monotonic_time_ + std::chrono::milliseconds(5), time_system_.monotonicTime());

  // But going backward does not.
  time_system_.setMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(3));
  EXPECT_EQ(start_monotonic_time_ + std::chrono::milliseconds(5), time_system_.monotonicTime());
}

TEST_P(SimulatedTimeSystemTest, System) {
  // Setting time forward works.
  time_system_.setSystemTime(start_system_time_ + std::chrono::milliseconds(5));
  EXPECT_EQ(start_system_time_ + std::chrono::milliseconds(5), time_system_.systemTime());

  // And going backward works too.
  time_system_.setSystemTime(start_system_time_ + std::chrono::milliseconds(3));
  EXPECT_EQ(start_system_time_ + std::chrono::milliseconds(3), time_system_.systemTime());
}

TEST_P(SimulatedTimeSystemTest, Ordering) {
  addTask(5, '5');
  addTask(3, '3');
  addTask(6, '6');
  EXPECT_EQ("", output_);
  advanceMsAndLoop(5);
  EXPECT_EQ("35", output_);
  advanceMsAndLoop(1);
  EXPECT_EQ("356", output_);
}

TEST_P(SimulatedTimeSystemTest, SystemTimeOrdering) {
  addTask(5, '5');
  addTask(3, '3');
  addTask(6, '6');
  EXPECT_EQ("", output_);
  advanceSystemMsAndLoop(5);
  EXPECT_EQ("35", output_);
  advanceSystemMsAndLoop(1);
  EXPECT_EQ("356", output_);
  time_system_.setSystemTime(start_system_time_ + std::chrono::milliseconds(1));
  time_system_.setSystemTime(start_system_time_ + std::chrono::milliseconds(100));
  EXPECT_EQ("356", output_); // callbacks don't get replayed.
}

TEST_P(SimulatedTimeSystemTest, DisableTimer) {
  addTask(5, '5');
  addTask(3, '3');
  addTask(6, '6');
  timers_[0]->disableTimer();
  EXPECT_EQ("", output_);
  advanceMsAndLoop(5);
  EXPECT_EQ("3", output_);
  advanceMsAndLoop(1);
  EXPECT_EQ("36", output_);
}

TEST_P(SimulatedTimeSystemTest, IgnoreRedundantDisable) {
  addTask(5, '5');
  timers_[0]->disableTimer();
  timers_[0]->disableTimer();
  advanceMsAndLoop(5);
  EXPECT_EQ("", output_);
}

TEST_P(SimulatedTimeSystemTest, OverrideEnable) {
  addTask(5, '5');
  timers_[0]->enableTimer(std::chrono::milliseconds(6));
  advanceMsAndLoop(5);
  EXPECT_EQ("", output_); // Timer didn't wake up because we overrode to 6ms.
  advanceMsAndLoop(1);
  EXPECT_EQ("5", output_);
}

TEST_P(SimulatedTimeSystemTest, DeleteTime) {
  addTask(5, '5');
  addTask(3, '3');
  addTask(6, '6');
  timers_[0].reset();
  EXPECT_EQ("", output_);
  advanceMsAndLoop(5);
  EXPECT_EQ("3", output_);
  advanceMsAndLoop(1);
  EXPECT_EQ("36", output_);
}

// Regression test for issues documented in https://github.com/envoyproxy/envoy/pull/6956
TEST_P(SimulatedTimeSystemTest, DuplicateTimer) {
  // Set one alarm two times to test that pending does not get duplicated..
  std::chrono::milliseconds delay(0);
  TimerPtr zero_timer = scheduler_->createTimer([this]() { output_.append(1, '2'); }, dispatcher_);
  zero_timer->enableTimer(delay);
  zero_timer->enableTimer(delay);
  advanceMsAndLoop(1);
  EXPECT_EQ("2", output_);

  // Now set an alarm which requires 10ms of progress and make sure waitFor works.
  std::atomic<bool> done(false);
  auto thread = Thread::threadFactoryForTest().createThread([this, &done]() {
    while (!done) {
      base_scheduler_.run(Dispatcher::RunType::Block);
    }
  });
  Thread::MutexBasicLockable mutex;
  Thread::CondVar condvar;
  TimerPtr timer = scheduler_->createTimer(
      [&condvar, &mutex, &done]() {
        Thread::LockGuard lock(mutex);
        done = true;
        condvar.notifyOne();
      },
      dispatcher_);
  timer->enableTimer(std::chrono::seconds(10));

  {
    Thread::LockGuard lock(mutex);
    time_system_.waitFor(mutex, condvar, std::chrono::seconds(10));
  }
  EXPECT_TRUE(done);

  thread->join();
}

TEST_P(SimulatedTimeSystemTest, Enabled) {
  TimerPtr timer = scheduler_->createTimer({}, dispatcher_);
  timer->enableTimer(std::chrono::milliseconds(0));
  EXPECT_TRUE(timer->enabled());
}

} // namespace
} // namespace Test
} // namespace Event
} // namespace Envoy
