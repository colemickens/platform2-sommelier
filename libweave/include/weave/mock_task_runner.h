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
  MockTaskRunner();
  ~MockTaskRunner() override;

  MOCK_METHOD3(PostDelayedTask,
               void(const tracked_objects::Location&,
                    const base::Closure&,
                    base::TimeDelta));

  bool RunOnce();
  void Run();
  base::Clock* GetClock();

 private:
  void SaveTask(const tracked_objects::Location& from_here,
                const base::Closure& task,
                base::TimeDelta delay);

  using QueueItem = std::pair<std::pair<base::Time, size_t>, base::Closure>;

  struct Greater {
    bool operator()(const QueueItem& a, const QueueItem& b) const {
      return a.first > b.first;
    }
  };

  size_t counter_{0};  // Keeps order of tasks with the same time.

  class TestClock;
  std::unique_ptr<TestClock> test_clock_;

  std::priority_queue<QueueItem,
                      std::vector<QueueItem>,
                      MockTaskRunner::Greater> queue_;
};

}  // namespace unittests
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_MOCK_TASK_RUNNER_H_
