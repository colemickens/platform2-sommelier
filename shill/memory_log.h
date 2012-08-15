// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MEMORY_LOG_H_
#define SHILL_MEMORY_LOG_H_

#include <deque>
#include <string>
#include <sstream>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/lazy_instance.h>
#include <base/logging.h>
#include <gtest/gtest_prod.h>

class FilePath;

// MemoryLog is nothing but a memory buffer of the most recent messages, capped
// by a configurable limit of how many message bytes to remember at a time.
// When a new message comes in, we add it to the buffer, then drop the oldest
// messages until the size of the buffer is under the byte limit.  The number of
// bytes in the buffer does not include std::string overhead, nor overhead from
// the buffer implementation.  Only bytes in messages are counted.
//
// When something 'interesting' happens (e.g. connectivity event or crash), the
// logic reacting to that event can dump the contents of the MemoryLog to disk.
// This gives us a verbose log of the most recent events up until the event,
// which may be useful for further debugging.

namespace shill {

class MemoryLog {
 public:
  // Returns a singleton of this class.
  static MemoryLog *GetInstance();

  ~MemoryLog();
  // Appends this message to the log, dropping the oldest messages until the log
  // is under the byte limit.
  void Append(const std::string &msg);
  // Removes all messages from the log.
  void Clear();
  // Sets the maximum size for the log and drops messages until we get under it.
  void SetMaximumSize(size_t size_in_bytes);
  // See FlushToDiskImpl().
  void FlushToDisk();

  size_t maximum_size_bytes() const { return maximum_size_bytes_; }
  size_t current_size_bytes() const { return current_size_bytes_; }

 private:
  friend class MemoryLogTest;
  // Required for constructing LazyInstance<MemoryLog>.
  friend struct base::DefaultLazyInstanceTraits<MemoryLog>;
  FRIEND_TEST(ManagerTest,   PopProfileShouldClearMemoryLog);
  FRIEND_TEST(MemoryLogTest, MemoryLogFlushToDiskCannotCreateFile);
  FRIEND_TEST(MemoryLogTest, MemoryLogFlushToDiskRotateWorks);
  FRIEND_TEST(MemoryLogTest, MemoryLogFlushToDiskWorks);
  FRIEND_TEST(MemoryLogTest, MemoryLogFlushToFileWorks);
  FRIEND_TEST(MemoryLogTest, MemoryLogIsLogging);
  FRIEND_TEST(MemoryLogTest, MemoryLogLimitingWorks);
  FRIEND_TEST(MemoryLogTest, MemoryLogMessageInterceptorWorks);

  // Arbitrary default verbose log capacity is an even megabyte.
  static const size_t kDefaultMaximumMemoryLogSizeInBytes = 1 << 20;
  // Default log dump path used with FlushToDisk() when a user is logged in.
  static const char kDefaultLoggedInDumpPath[];
  // Default log dump path used when no user is logged in.
  static const char kDefaultLoggedOutDumpPath[];
  // If this file exists, then we say that a user is logged in
  static const char kLoggedInTokenPath[];
  // The on disk log file may only be this big before we'll forcibly rotate it.
  // This means we may have this number * 2 bytes on disk at any time.
  static const size_t kDefaultMaxDiskLogSizeInBytes =
      kDefaultMaximumMemoryLogSizeInBytes * 20;

  std::deque<std::string> log_;

  void ShrinkToTargetSize(size_t number_bytes);
  size_t TestGetNumberMessages() { return log_.size(); }
  bool TestContainsMessageWithText(const char *msg);
  void TestSetMaxDiskLogSize(size_t number_bytes) {
    maximum_disk_log_size_bytes_ = number_bytes;
  }
  // Appends the current contents of the memory buffer to a specified file on
  // disk. Returns the number of bytes written to disk, or -1 on failure.  -1 is
  // returned on failure even if some bytes have already made it to disk.
  // Attempts to create the parent directories of |file_path| if it does not
  // already exist.  If the resulting log file is too large (> kMaxDiskLogSize),
  // tries to rotate logs.
  ssize_t FlushToFile(const FilePath &file_path);
  // Flushes the log to disk via FlushToFile, then clears the log, and tries to
  // rotate our logs if |file_path| is larger than
  // |maximum_disk_log_size_bytes_|.
  //
  // We rotate here rather than through logrotate because we fear situations
  // where we experience a lot of connectivity problems in a short span of time
  // before logrotate has a chance to run.
  void FlushToDiskImpl(const FilePath &file_path);

  size_t maximum_size_bytes_;
  size_t current_size_bytes_;
  size_t maximum_disk_log_size_bytes_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryLog);
};

class MemoryLogMessage {
 public:
  MemoryLogMessage(const char *file,
                   int line,
                   logging::LogSeverity severity,
                   bool propagate_down);
  ~MemoryLogMessage();

  std::ostream &stream() { return stream_; }

 private:
  const char *file_;
  const int line_;
  const logging::LogSeverity severity_;
  bool propagate_down_;
  std::ostringstream stream_;
  size_t message_start_;

  void Init();

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryLogMessage);
};

}  // namespace shill

#endif  // SHILL_MEMORY_LOG_H_
