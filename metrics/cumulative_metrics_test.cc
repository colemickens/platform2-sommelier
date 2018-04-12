// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "metrics/cumulative_metrics.h"
#include "metrics/persistent_integer_test_base.h"

using chromeos_metrics::CumulativeMetrics;

namespace chromeos_metrics {
namespace {

constexpr char kMetricNameX[] = "x.pi";
constexpr char kMetricNameY[] = "y.pi";

}  // namespace

class CumulativeMetricsTest : public PersistentIntegerTestBase {
};

static void UpdateAccumulators(CumulativeMetrics *cm) {
  cm->Add(kMetricNameX, 100);
  cm->Add(kMetricNameY, 200);
}

static void ReportAccumulators(CumulativeMetrics *cm) {
  static int call_count = 0;

  // The first call is done at initialization, to possibly report metrics
  // accumulated in the previous cycle, so we ignore it.
  if (call_count == 1) {
    EXPECT_EQ(cm->Get(kMetricNameX), 500);
    EXPECT_EQ(cm->Get(kMetricNameY), 1000);
  }

  call_count += 1;
}

TEST_F(CumulativeMetricsTest, TestLoop) {
  base::MessageLoop message_loop;

  std::vector<std::string> names = {kMetricNameX, kMetricNameY};
  CumulativeMetrics cm(
      "cumulative.testing.",
      names,
      base::TimeDelta::FromMilliseconds(100),
      base::Bind(&UpdateAccumulators),
      base::TimeDelta::FromMilliseconds(500),
      base::Bind(&ReportAccumulators));

  // Schedule a time to quit, after the 5th call to UpdateAccumulators, but
  // before the 6th.
  message_loop.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&base::MessageLoop::QuitNow, base::Unretained(&message_loop)),
      base::TimeDelta::FromMicroseconds(590000));

  base::RunLoop().Run();

  EXPECT_EQ(cm.Get(kMetricNameX), 0);
  EXPECT_EQ(cm.Get(kMetricNameY), 0);
}

}  // namespace chromeos_metrics
