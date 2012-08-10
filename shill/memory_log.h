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
  // Prefix prepended to every message passing through MemoryLog.
  static const char kMemoryLogPrefix[];

  // Returns a singleton of this class.
  static MemoryLog *GetInstance();
  // Installs a message handler that traps log messages that evaded MemoryLog
  // earlier.  These messages come from places like *CHECK, NOT_IMPLEMENTED, and
  // similar logging calls.  This will attempt to save the previous handler and
  // call it recursively.  It is the callers responsibility to ensure that no
  // other thread is logging or touching the log handlers at the same time.
  static void InstallLogInterceptor();
  // Reinstalls the message handler in place when our interceptor was installed.
  // It is up to the caller to ensure that no logging takes place during this
  // call, and no other threads are touching the log message handlers.  The
  // caller is also responsible for guaranteeing our handler is uninstalled in
  // the reverse order it was installed in (LIFO).
  static void UninstallLogInterceptor();

  ~MemoryLog();
  // Appends this message to the log, dropping the oldest messages until the log
  // is under the byte limit.
  void Append(const std::string &msg);
  // Removes all messages from the log.
  void Clear();
  // Sends the current contents of the memory buffer to a specified file on
  // disk. Returns the number of bytes written to disk, or -1 on failure.  -1 is
  // returned on failure even if some bytes have already made it to disk.
  ssize_t FlushToDisk(const std::string &file_path);
  // Sets the maximum size for the log and drops messages until we get under it.
  void SetMaximumSize(size_t size_in_bytes);

  size_t maximum_size_bytes() const { return maximum_size_bytes_; }
  size_t current_size_bytes() const { return current_size_bytes_; }

 private:
  friend class MemoryLogTest;
  // Required for constructing LazyInstance<MemoryLog>.
  friend struct base::DefaultLazyInstanceTraits<MemoryLog>;
  FRIEND_TEST(MemoryLogTest, MemoryLogFlushToDiskWorks);
  FRIEND_TEST(MemoryLogTest, MemoryLogIsLogging);
  FRIEND_TEST(MemoryLogTest, MemoryLogLimitingWorks);
  FRIEND_TEST(MemoryLogTest, MemoryLogMessageInterceptorWorks);

  // Arbitrary default verbose log capacity is an even megabyte.
  static const size_t kDefaultMaximumMemoryLogSizeInBytes = 1 << 20;

  std::deque<std::string> log_;

  void ShrinkToTargetSize(size_t number_bytes);
  size_t TestGetNumberMessages() { return log_.size(); }

  size_t maximum_size_bytes_;
  size_t current_size_bytes_;

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
