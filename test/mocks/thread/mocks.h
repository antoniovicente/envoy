#pragma once

#include "envoy/thread/thread.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Thread {

class MockThreadFactory : public ThreadFactory {
public:
  MOCK_METHOD(ThreadPtr, createThread,
              (std::function<void()> thread_routine, OptionsOptConstRef options));
  MOCK_METHOD(ThreadId, currentThreadId, ());
};

} // namespace Thread
} // namespace Envoy
