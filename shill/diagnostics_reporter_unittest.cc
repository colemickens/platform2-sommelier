// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/diagnostics_reporter.h"

#include <base/message_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"

using testing::Return;

namespace shill {

class DiagnosticsReporterTest : public testing::Test {
 public:
  DiagnosticsReporterTest() {}

 protected:
  bool IsReportingEnabled() {
    return DiagnosticsReporter::GetInstance()->IsReportingEnabled();
  }
};

namespace {

class ReporterUnderTest : public DiagnosticsReporter {
 public:
  ReporterUnderTest() {}

  MOCK_METHOD0(IsReportingEnabled, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(ReporterUnderTest);
};

}  // namespace

TEST_F(DiagnosticsReporterTest, Report) {
  // The test is pretty basic but covers the main flow and ensures that the main
  // process doesn't crash.
  ReporterUnderTest reporter;
  EXPECT_CALL(reporter, IsReportingEnabled())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EventDispatcher dispatcher;
  reporter.Init(&dispatcher);
  reporter.Report();
  reporter.Report();
  dispatcher.PostTask(MessageLoop::QuitClosure());
  dispatcher.DispatchForever();
}

TEST_F(DiagnosticsReporterTest, IsReportingEnabled) {
  EXPECT_FALSE(IsReportingEnabled());
}

}  // namespace shill
