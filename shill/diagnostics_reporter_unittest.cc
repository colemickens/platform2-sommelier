// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/diagnostics_reporter.h"

#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_minijail.h"
#include "shill/mock_process_killer.h"
#include "shill/mock_time.h"

using base::FilePath;
using testing::_;
using testing::ElementsAre;
using testing::InSequence;
using testing::IsNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;

namespace shill {

namespace {

class ReporterUnderTest : public DiagnosticsReporter {
 public:
  ReporterUnderTest() {}

  MOCK_METHOD0(IsReportingEnabled, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(ReporterUnderTest);
};

}  // namespace

class DiagnosticsReporterTest : public testing::Test {
 public:
  DiagnosticsReporterTest() {
    reporter_.minijail_ = &minijail_;
    reporter_.process_killer_ = &process_killer_;
    reporter_.time_ = &time_;
  }

 protected:
  bool IsReportingEnabled() {
    return DiagnosticsReporter::GetInstance()->IsReportingEnabled();
  }

  void SetLastLogStash(uint64 time) { reporter_.last_log_stash_ = time; }
  uint64 GetLastLogStash() { return reporter_.last_log_stash_; }
  uint64 GetLogStashThrottleSeconds() {
    return DiagnosticsReporter::kLogStashThrottleSeconds;
  }

  void SetStashedNetLog(const FilePath &stashed_net_log) {
    reporter_.stashed_net_log_ = stashed_net_log;
  }

  MockMinijail minijail_;
  MockProcessKiller process_killer_;
  MockTime time_;
  ReporterUnderTest reporter_;
};

TEST_F(DiagnosticsReporterTest, IsReportingEnabled) {
  EXPECT_FALSE(IsReportingEnabled());
}

TEST_F(DiagnosticsReporterTest, OnConnectivityEventThrottle) {
  const uint64 kLastStash = 50;
  const uint64 kNow = kLastStash + GetLogStashThrottleSeconds() - 1;
  SetLastLogStash(kLastStash);
  const struct timeval now = {
    .tv_sec = static_cast<long int>(kNow),
    .tv_usec = 0
  };
  EXPECT_CALL(time_, GetTimeMonotonic(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(now), Return(0)));
  reporter_.OnConnectivityEvent();
  EXPECT_EQ(kLastStash, GetLastLogStash());
}

TEST_F(DiagnosticsReporterTest, OnConnectivityEvent) {
  const uint64 kInitStash = 0;
  SetLastLogStash(kInitStash);
  // Test that the initial call is not throttled.
  const uint64 kNow0 = kInitStash + 1;
  const struct timeval now0 = {
    .tv_sec = static_cast<long int>(kNow0),
    .tv_usec = 0
  };
  const uint64 kNow1 = kNow0 + GetLogStashThrottleSeconds() + 1;
  const struct timeval now1 = {
    .tv_sec = static_cast<long int>(kNow1),
    .tv_usec = 0
  };
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath stashed_net_log = temp_dir_.path().Append("stashed-net-log");
  EXPECT_EQ(0, file_util::WriteFile(stashed_net_log, "", 0));
  EXPECT_TRUE(file_util::PathExists(stashed_net_log));
  SetStashedNetLog(stashed_net_log);
  EXPECT_CALL(time_, GetTimeMonotonic(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(now0), Return(0)))
      .WillOnce(DoAll(SetArgumentPointee<0>(now1), Return(0)));
  EXPECT_CALL(reporter_, IsReportingEnabled())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(minijail_, New()).Times(2);
  EXPECT_CALL(minijail_, DropRoot(_, StrEq("syslog"))).Times(2);
  const pid_t kPID = 123;
  {
    InSequence s;
    EXPECT_CALL(minijail_,
                RunAndDestroy(_, ElementsAre(_, IsNull()), _))
        .WillOnce(DoAll(SetArgumentPointee<2>(kPID), Return(true)));
    EXPECT_CALL(
        minijail_,
        RunAndDestroy(_, ElementsAre(_, StrEq("--upload"), IsNull()), _))
        .WillOnce(Return(false));
  }
  EXPECT_CALL(process_killer_, Wait(kPID, _)).Times(1);
  reporter_.OnConnectivityEvent();
  EXPECT_EQ(kNow0, GetLastLogStash());
  EXPECT_FALSE(file_util::PathExists(stashed_net_log));
  reporter_.OnConnectivityEvent();
  EXPECT_EQ(kNow1, GetLastLogStash());
}

}  // namespace shill
