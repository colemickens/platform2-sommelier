// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>

#include <base/file_path.h>
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

class MemoryLogDeathTest : public testing::Test { };

TEST_F(MemoryLogTest, ScopedLoggerStillWorks) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, kTestStr1));
  EXPECT_CALL(log, Log(_, _, kTestStr2));
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
  EXPECT_CALL(log, Log(_, _, kTestStr1));
  EXPECT_CALL(log, Log(_, _, kTestStr2));
  LOG(ERROR) << kTestStr1;
  LOG(INFO) << "does not propagate down";
  // It would be nice if the compiler didn't optimize out my conditional check.
  LOG_IF(WARNING, strlen("two") == 3) << kTestStr2;
}

// Test that no matter what we did, CHECK still kills the process.
TEST_F(MemoryLogDeathTest, CheckLogsStillWork) {
  EXPECT_DEATH( { CHECK(false) << "diediedie"; },
                "Check failed: false. diediedie");
}

TEST_F(MemoryLogTest, MemoryLogIsLogging) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, kTestStr1));
  EXPECT_CALL(log, Log(_, _, kTestStr2));
  ::logging::SetMinLogLevel(logging::LOG_WARNING);
  ASSERT_EQ(0, MemoryLog::GetInstance()->current_size_bytes());
  LOG(WARNING) << kTestStr1;
  LOG(WARNING) << kTestStr2;
  // LT because of the prefixes prepended by the logger
  EXPECT_LT(strlen(kTestStr1) + strlen(kTestStr2),
            MemoryLog::GetInstance()->current_size_bytes());
  EXPECT_EQ(2, MemoryLog::GetInstance()->TestGetNumberMessages());
  MemoryLog::GetInstance()->Clear();
  EXPECT_EQ(0, MemoryLog::GetInstance()->current_size_bytes());
  EXPECT_EQ(0, MemoryLog::GetInstance()->TestGetNumberMessages());
}

TEST_F(MemoryLogTest, MemoryLogLimitingWorks) {
  ScopedMockLog log;
  ::logging::SetMinLogLevel(logging::LOG_WARNING);
  LOG(INFO) << kTestStr1;
  size_t old_size = MemoryLog::GetInstance()->current_size_bytes();
  LOG(INFO) << kTestStr2;
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
  EXPECT_CALL(log, Log(_, _, kTestStr3));
  LOG(WARNING) << kTestStr3;
  ASSERT_EQ(0, MemoryLog::GetInstance()->current_size_bytes());
  ASSERT_EQ(0, MemoryLog::GetInstance()->TestGetNumberMessages());
}

TEST_F(MemoryLogTest, MemoryLogFlushToFileWorks) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath test_path = temp_dir.path().Append("somelogfile");
  ::logging::SetMinLogLevel(logging::LOG_WARNING);
  LOG(INFO) << kTestStr1;
  LOG(INFO) << kTestStr2;
  LOG(INFO) << kTestStr3;
  EXPECT_EQ(3, MemoryLog::GetInstance()->TestGetNumberMessages());
  // Because of all the prefixed metadata on each log message, the stuff sent to
  // disk should be bigger than the original strings put together.
  size_t minimal_message_length = strlen(kTestStr1) + strlen(kTestStr2) +
      strlen(kTestStr3);
  EXPECT_LT(minimal_message_length,
            MemoryLog::GetInstance()->FlushToFile(test_path));
  std::string file_contents;
  EXPECT_TRUE(file_util::ReadFileToString(test_path, &file_contents));
  // Log should contain all three messages
  EXPECT_NE(file_contents.find(kTestStr1), std::string::npos);
  EXPECT_NE(file_contents.find(kTestStr2), std::string::npos);
  EXPECT_NE(file_contents.find(kTestStr3), std::string::npos);
  // Preserve message order
  EXPECT_LT(file_contents.find(kTestStr1), file_contents.find(kTestStr2));
  EXPECT_LT(file_contents.find(kTestStr2), file_contents.find(kTestStr3));
  EXPECT_TRUE(temp_dir.Delete());
}

TEST_F(MemoryLogTest, MemoryLogFlushToDiskCannotCreateFile) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_,
                       _,
                       "Failed to open file for dumping memory log to disk."));
  EXPECT_CALL(log, Log(_, _, "Failed to flush memory log to disk"));
  FilePath tmp_path;
  file_util::CreateTemporaryFile(&tmp_path);
  // Flushing fails because a directory already exists with the same name as
  // our log file.
  MemoryLog::GetInstance()->FlushToDiskImpl(
      tmp_path.Append("cannot_be_created"));
}

TEST_F(MemoryLogTest, MemoryLogFlushToDiskRotateWorks) {
  FilePath tmp_dir;
  file_util::CreateNewTempDirectory(std::string("/"), &tmp_dir);
  FilePath log_path = tmp_dir.Append("connectivity.log");
  FilePath log_path_backup = tmp_dir.Append("connectivity.bak");
  LOG(INFO) << kTestStr1;
  LOG(INFO) << kTestStr2;
  // Populate a dump file with some messages.
  MemoryLog::GetInstance()->FlushToDiskImpl(log_path);
  // There should be no rotated file at this point, we've only done one dump.
  EXPECT_FALSE(file_util::PathExists(log_path_backup));
  // Tell MemoryLog it should rotate at a really small size threshhold.
  MemoryLog::GetInstance()->TestSetMaxDiskLogSize(1);
  LOG(INFO) << kTestStr3;
  // Flush to disk, which should cause a log rotate, since the old log file had
  // more than a byte in it.
  MemoryLog::GetInstance()->FlushToDiskImpl(log_path);
  MemoryLog::GetInstance()->TestSetMaxDiskLogSize(
      MemoryLog::kDefaultMaxDiskLogSizeInBytes);
  std::string file_contents;
  EXPECT_TRUE(file_util::ReadFileToString(log_path_backup, &file_contents));
  // Rotated log should contain the first two messages.
  EXPECT_NE(file_contents.find(kTestStr1), std::string::npos);
  EXPECT_NE(file_contents.find(kTestStr2), std::string::npos);
  EXPECT_TRUE(file_util::Delete(tmp_dir, true));
}

TEST_F(MemoryLogTest, MemoryLogFlushToDiskWorks) {
  FilePath tmp_path;
  file_util::CreateTemporaryFile(&tmp_path);
  LOG(INFO) << kTestStr1;
  LOG(INFO) << kTestStr2;
  LOG(INFO) << kTestStr3;
  MemoryLog::GetInstance()->FlushToDiskImpl(tmp_path);
  // No rotation should have happened.
  EXPECT_FALSE(file_util::PathExists(tmp_path.Append(".bak")));
  // But we should have a dump file now.
  std::string file_contents;
  EXPECT_TRUE(file_util::ReadFileToString(tmp_path, &file_contents));
  // Dump file should contain everything we logged.
  EXPECT_NE(file_contents.find(kTestStr1), std::string::npos);
  EXPECT_NE(file_contents.find(kTestStr2), std::string::npos);
  EXPECT_NE(file_contents.find(kTestStr3), std::string::npos);
  EXPECT_TRUE(file_util::Delete(tmp_path, false));
}

// Test that most messages go through MemoryLog
TEST_F(MemoryLogTest, MemoryLogMessageInterceptorWorks) {
  ASSERT_EQ(0, MemoryLog::GetInstance()->TestGetNumberMessages());
  // Make sure we're not double logging.
  LOG(ERROR) << kTestStr1;
  ASSERT_EQ(1, MemoryLog::GetInstance()->TestGetNumberMessages());
  SLOG_IF(WiFi, 3, strlen("two") == 3) << kTestStr2;
  ASSERT_EQ(2, MemoryLog::GetInstance()->TestGetNumberMessages());
  SLOG_IF(WiFi, 3, strlen("one") == 2) << "does not get through again";
  ASSERT_EQ(2, MemoryLog::GetInstance()->TestGetNumberMessages());
  LOG_IF(ERROR, strlen("two") == 3) << kTestStr2;
  ASSERT_EQ(3, MemoryLog::GetInstance()->TestGetNumberMessages());
  LOG_IF(ERROR, strlen("one") == 2) << "does not get through again";
  ASSERT_EQ(3, MemoryLog::GetInstance()->TestGetNumberMessages());
  NOTIMPLEMENTED();
  ASSERT_EQ(4, MemoryLog::GetInstance()->TestGetNumberMessages());

}

}  // namespace shill
