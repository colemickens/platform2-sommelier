// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_MOCK_TASK_RUNNER_H_
#define LIBWEAVE_INCLUDE_WEAVE_MOCK_TASK_RUNNER_H_

#include <weave/task_runner.h>

#include <algorithm>
#include <queue>
#include <utility>
#include <vector>

#include <base/time/clock.h>
#include <gmock/gmock.h>

namespace weave {
namespace unittests {

class MockTaskRunner : public TaskRunner {
 public:
  MockTaskRunner() {
    test_clock_.SetNow(base::Time::Now());
    using testing::_;
    using testing::Invoke;
    using testing::AnyNumber;
    ON_CALL(*this, PostDelayedTask(_, _, _))
        .WillByDefault(Invoke(this, &MockTaskRunner::SaveTask));
    EXPECT_CALL(*this, PostDelayedTask(_, _, _)).Times(AnyNumber());
  }
  ~MockTaskRunner() override = default;

  MOCK_METHOD3(PostDelayedTask,
               void(const tracked_objects::Location&,
                    const base::Closure&,
                    base::TimeDelta));

  bool RunOnce() {
    if (queue_.empty())
      return false;
    auto top = queue_.top();
    queue_.pop();
    test_clock_.SetNow(std::max(test_clock_.Now(), top.first.first));
    top.second.Run();
    return true;
  }

  void Run() {
    while (RunOnce()) {
    }
  }

  base::Clock* GetClock() { return &test_clock_; }

 private:
  void SaveTask(const tracked_objects::Location& from_here,
                const base::Closure& task,
                base::TimeDelta delay) {
    queue_.emplace(std::make_pair(test_clock_.Now() + delay, ++counter_), task);
  }

  using QueueItem = std::pair<std::pair<base::Time, size_t>, base::Closure>;

  struct Greater {
    bool operator()(const QueueItem& a, const QueueItem& b) const {
      return a.first > b.first;
    }
  };

  size_t counter_{0};  // Keeps order of tasks with the same time.

  class TestClock : public base::Clock {
   public:
    base::Time Now() override { return now_; }

    void SetNow(base::Time now) { now_ = now; }

   private:
    base::Time now_;
  };
  TestClock test_clock_;

  std::priority_queue<QueueItem,
                      std::vector<QueueItem>,
                      MockTaskRunner::Greater> queue_;
};

}  // namespace unittests
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_MOCK_TASK_RUNNER_H_
