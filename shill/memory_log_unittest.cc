// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>

#include <base/file_util.h>
#include <base/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/logging.h"
#include "shill/mock_log.h"

using testing::_;

namespace {

const char kTestStr1[] = "What does Mr Wallace look like?";
const char kTestStr2[] = "And now little man, I give the watch to you.";
const char kTestStr3[] = "This is a tasty burger!";
const char kTestStr1WithPrefix[] = "memlog: What does Mr Wallace look like?";
const char kTestStr2WithPrefix[] =
    "memlog: And now little man, I give the watch to you.";
const char kTestStr3WithPrefix[] = "memlog: This is a tasty burger!";

}  // namespace

namespace shill {

class MemoryLogTest : public testing::Test {
 protected:
  void TearDown() {
    // Restore everything to defaults once more.
    MemoryLog::GetInstance()->Clear();
    MemoryLog::GetInstance()->SetMaximumSize(
        MemoryLog::kDefaultMaximumMemoryLogSizeInBytes);
    ScopeLogger::GetInstance()->set_verbose_level(0);
    ScopeLogger::GetInstance()->EnableScopesByName("");
    ::logging::SetMinLogLevel(logging::LOG_INFO);
  }
};

TEST_F(MemoryLogTest, ScopedLoggerStillWorks) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, kTestStr1WithPrefix));
  EXPECT_CALL(log, Log(_, _, kTestStr2WithPrefix));
  SLOG(WiFi, 2) << "does not get through";
  ScopeLogger::GetInstance()->EnableScopesByName("+wifi");
  // Verbose levels are inverted.
  ScopeLogger::GetInstance()->set_verbose_level(3);
  SLOG(WiFi, 2) << kTestStr1;
  // It would be nice if the compiler didn't optimize out my conditional check.
  SLOG_IF(WiFi, 3, strlen("two") == 3) << kTestStr2;
  SLOG_IF(WiFi, 3, strlen("one") == 2) << "does not get through again";
  SLOG(WiFi, 4) << "spanish inquisition";
}

TEST_F(MemoryLogTest, NormalLoggingStillWorks) {
  ::logging::SetMinLogLevel(logging::LOG_WARNING);
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, kTestStr1WithPrefix));
  EXPECT_CALL(log, Log(_, _, kTestStr2WithPrefix));
  MLOG(ERROR) << kTestStr1;
  MLOG(INFO) << "does not propagate down";
  // It would be nice if the compiler didn't optimize out my conditional check.
  MLOG_IF(WARNING, strlen("two") == 3) << kTestStr2;
}

TEST_F(MemoryLogTest, MemoryLogIsLogging) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, kTestStr1WithPrefix));
  EXPECT_CALL(log, Log(_, _, kTestStr2WithPrefix));
  ::logging::SetMinLogLevel(logging::LOG_WARNING);
  ASSERT_EQ(0, MemoryLog::GetInstance()->current_size_bytes());
  MLOG(WARNING) << kTestStr1;
  MLOG(WARNING) << kTestStr2;
  // LT because of the prefixes prepended by the logger
  ASSERT_LT(strlen(kTestStr1WithPrefix) + strlen(kTestStr2WithPrefix),
            MemoryLog::GetInstance()->current_size_bytes());
  ASSERT_EQ(2, MemoryLog::GetInstance()->TestGetNumberMessages());
  MemoryLog::GetInstance()->Clear();
  ASSERT_EQ(0, MemoryLog::GetInstance()->current_size_bytes());
  ASSERT_EQ(0, MemoryLog::GetInstance()->TestGetNumberMessages());
}

TEST_F(MemoryLogTest, MemoryLogLimitingWorks) {
  ScopedMockLog log;
  ::logging::SetMinLogLevel(logging::LOG_WARNING);
  MLOG(INFO) << kTestStr1;
  size_t old_size = MemoryLog::GetInstance()->current_size_bytes();
  MLOG(INFO) << kTestStr2;
  size_t new_size = MemoryLog::GetInstance()->current_size_bytes();
  // Setting the size just above the current size shouldn't affect anything.
  MemoryLog::GetInstance()->SetMaximumSize(new_size + 1);
  ASSERT_EQ(new_size, MemoryLog::GetInstance()->current_size_bytes());
  // Force MemoryLog to discard the earliest message
  MemoryLog::GetInstance()->SetMaximumSize(new_size - 1);
  // Should be just last message in the buffer.
  ASSERT_EQ(new_size - old_size,
            MemoryLog::GetInstance()->current_size_bytes());
  // Now force it to discard the most recent message.
  MemoryLog::GetInstance()->SetMaximumSize(0);
  ASSERT_EQ(0, MemoryLog::GetInstance()->current_size_bytes());
  // Can't log if we don't have room, but the messages should still get to LOG
  EXPECT_CALL(log, Log(_, _, kTestStr3WithPrefix));
  MLOG(WARNING) << kTestStr3;
  ASSERT_EQ(0, MemoryLog::GetInstance()->current_size_bytes());
  ASSERT_EQ(0, MemoryLog::GetInstance()->TestGetNumberMessages());
}

TEST_F(MemoryLogTest, MemoryLogFlushToDiskWorks) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath test_path = temp_dir.path().Append("somelogfile");
  ::logging::SetMinLogLevel(logging::LOG_WARNING);
  MLOG(INFO) << kTestStr1;
  MLOG(INFO) << kTestStr2;
  MLOG(INFO) << kTestStr3;
  ASSERT_EQ(3, MemoryLog::GetInstance()->TestGetNumberMessages());
  // Because of all the prefixed metadata on each log message, the stuff sent to
  // disk should be bigger than the original strings put together.
  size_t minimal_message_length = strlen(kTestStr1WithPrefix) +
      strlen(kTestStr2WithPrefix) + strlen(kTestStr3WithPrefix);
  ASSERT_LT(minimal_message_length,
            MemoryLog::GetInstance()->FlushToDisk(test_path.value().c_str()));
  std::string file_contents;
  ASSERT_TRUE(file_util::ReadFileToString(test_path, &file_contents));
  // Log should contain all three messages
  ASSERT_NE(file_contents.find(kTestStr1WithPrefix), std::string::npos);
  ASSERT_NE(file_contents.find(kTestStr2WithPrefix), std::string::npos);
  ASSERT_NE(file_contents.find(kTestStr3WithPrefix), std::string::npos);
  // Preserve message order
  ASSERT_LT(file_contents.find(kTestStr1WithPrefix),
            file_contents.find(kTestStr2WithPrefix));
  ASSERT_LT(file_contents.find(kTestStr2WithPrefix),
            file_contents.find(kTestStr3WithPrefix));
  ASSERT_TRUE(temp_dir.Delete());
}

TEST_F(MemoryLogTest, MemoryLogMessageInterceptorWorks) {
  ASSERT_EQ(0, MemoryLog::GetInstance()->TestGetNumberMessages());
  NOTIMPLEMENTED();
  ASSERT_EQ(0, MemoryLog::GetInstance()->TestGetNumberMessages());
  MemoryLog::InstallLogInterceptor();
  NOTIMPLEMENTED();
  ASSERT_EQ(1, MemoryLog::GetInstance()->TestGetNumberMessages());
  // Make sure we're not double logging.
  MLOG(ERROR) << kTestStr1;
  ASSERT_EQ(2, MemoryLog::GetInstance()->TestGetNumberMessages());
  // SLOG_IF works with the intercepting handler.
  SLOG_IF(WiFi, 3, strlen("two") == 3) << kTestStr2;
  ASSERT_EQ(3, MemoryLog::GetInstance()->TestGetNumberMessages());
  SLOG_IF(WiFi, 3, strlen("one") == 2) << "does not get through again";
  ASSERT_EQ(3, MemoryLog::GetInstance()->TestGetNumberMessages());
  // Similarly, MLOG_IF works with the handler.
  MLOG_IF(ERROR, strlen("two") == 3) << kTestStr2;
  ASSERT_EQ(4, MemoryLog::GetInstance()->TestGetNumberMessages());
  MLOG_IF(ERROR, strlen("one") == 2) << "does not get through again";
  ASSERT_EQ(4, MemoryLog::GetInstance()->TestGetNumberMessages());
  MemoryLog::UninstallLogInterceptor();
  NOTIMPLEMENTED();
  ASSERT_EQ(4, MemoryLog::GetInstance()->TestGetNumberMessages());
  // Normal log messages still get through.
  MLOG(ERROR) << kTestStr2;
  ASSERT_EQ(5, MemoryLog::GetInstance()->TestGetNumberMessages());
}

}  // namespace shill
