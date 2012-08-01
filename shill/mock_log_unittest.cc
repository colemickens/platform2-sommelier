// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_log.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/scope_logger.h"

using ::std::string;
using ::testing::_;

namespace shill {

class MockLogTest : public testing::Test {
 protected:
  MockLogTest() {}

  void LogSomething(const string &message) const {
    LOG(INFO) << message;
  }
  void SlogSomething(const string &message) const {
    ScopeLogger::GetInstance()->EnableScopesByName("manager");
    ScopeLogger::GetInstance()->set_verbose_level(2);
    SLOG(Manager, 2) << message;
    ScopeLogger::GetInstance()->EnableScopesByName("-manager");
    ScopeLogger::GetInstance()->set_verbose_level(0);
  }
};

TEST_F(MockLogTest, MatchMessageOnly) {
  ScopedMockLog log;
  const string kMessage("Something");
  EXPECT_CALL(log, Log(_, _, kMessage));
  LogSomething(kMessage);
}

TEST_F(MockLogTest, MatchSeverityAndMessage) {
  ScopedMockLog log;
  const string kMessage("Something");
  EXPECT_CALL(log, Log(logging::LOG_INFO, _, kMessage));
  LogSomething(kMessage);
}

TEST_F(MockLogTest, MatchSeverityAndFileAndMessage) {
  ScopedMockLog log;
  const string kMessage("Something");
  EXPECT_CALL(log, Log(logging::LOG_INFO, "mock_log_unittest.cc", kMessage));
  LogSomething(kMessage);
}

TEST_F(MockLogTest, MatchEmptyString) {
  ScopedMockLog log;
  const string kMessage("");
  EXPECT_CALL(log, Log(_, _, kMessage));
  LogSomething(kMessage);
}

TEST_F(MockLogTest, MatchMessageContainsBracketAndNewline) {
  ScopedMockLog log;
  const string kMessage("blah [and more blah] \n yet more blah\n\n\n");
  EXPECT_CALL(log, Log(_, _, kMessage));
  LogSomething(kMessage);
}

TEST_F(MockLogTest, MatchSlog) {
  ScopedMockLog log;
  const string kMessage("Something");
  EXPECT_CALL(log, Log(_, _, kMessage));
  SlogSomething(kMessage);
}

TEST_F(MockLogTest, MatchWithGmockMatchers) {
  ScopedMockLog log;
  const string kMessage("Something");
  EXPECT_CALL(log, Log(::testing::Lt(::logging::LOG_ERROR),
                       ::testing::EndsWith(".cc"),
                       ::testing::StartsWith("Some")));
  LogSomething(kMessage);
}

}  // namespace shill
