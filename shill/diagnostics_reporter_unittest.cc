// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/diagnostics_reporter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_glib.h"

using testing::_;
using testing::Return;

namespace shill {

class DiagnosticsReporterTest : public testing::Test {
 public:
  DiagnosticsReporterTest() {}

 protected:
  bool IsReportingEnabled() {
    return DiagnosticsReporter::GetInstance()->IsReportingEnabled();
  }

  MockGLib glib_;
};

namespace {

class ReporterUnderTest : public DiagnosticsReporter {
 public:
  ReporterUnderTest() {}

  MOCK_METHOD0(IsReportingEnabled, bool());

  void Report() { DiagnosticsReporter::Report(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ReporterUnderTest);
};

}  // namespace

TEST_F(DiagnosticsReporterTest, ReportEnabled) {
  ReporterUnderTest reporter;
  EXPECT_CALL(reporter, IsReportingEnabled()).WillOnce(Return(true));
  EXPECT_CALL(glib_, SpawnSync(_, _, _, _, _, _, _, _, _, _)).Times(1);
  reporter.Init(&glib_);
  reporter.Report();
}

TEST_F(DiagnosticsReporterTest, ReportDisabled) {
  ReporterUnderTest reporter;
  EXPECT_CALL(reporter, IsReportingEnabled()).WillOnce(Return(false));
  EXPECT_CALL(glib_, SpawnSync(_, _, _, _, _, _, _, _, _, _)).Times(0);
  reporter.Init(&glib_);
  reporter.Report();
}

TEST_F(DiagnosticsReporterTest, IsReportingEnabled) {
  EXPECT_FALSE(IsReportingEnabled());
}

}  // namespace shill
