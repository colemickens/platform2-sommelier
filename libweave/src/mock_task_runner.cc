// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <weave/mock_task_runner.h>

using testing::_;
using testing::Invoke;
using testing::AnyNumber;

namespace weave {
namespace unittests {

class MockTaskRunner::TestClock : public base::Clock {
 public:
  base::Time Now() override { return now_; }

  void SetNow(base::Time now) { now_ = now; }

 private:
  base::Time now_{base::Time::Now()};
};

MockTaskRunner::MockTaskRunner() : test_clock_{new TestClock} {
  ON_CALL(*this, PostDelayedTask(_, _, _))
      .WillByDefault(Invoke(this, &MockTaskRunner::SaveTask));
  EXPECT_CALL(*this, PostDelayedTask(_, _, _)).Times(AnyNumber());
}

MockTaskRunner::~MockTaskRunner() {}

bool MockTaskRunner::RunOnce() {
  if (queue_.empty())
    return false;
  auto top = queue_.top();
  queue_.pop();
  test_clock_->SetNow(std::max(test_clock_->Now(), top.first.first));
  top.second.Run();
  return true;
}

void MockTaskRunner::Run() {
  while (RunOnce()) {
  }
}

base::Clock* MockTaskRunner::GetClock() {
  return test_clock_.get();
}

void MockTaskRunner::SaveTask(const tracked_objects::Location& from_here,
                              const base::Closure& task,
                              base::TimeDelta delay) {
  queue_.emplace(std::make_pair(test_clock_->Now() + delay, ++counter_), task);
}

}  // namespace unittests
}  // namespace weave
