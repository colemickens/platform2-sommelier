// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MEMORY_LOG_H_
#define SHILL_MEMORY_LOG_H_

#include <deque>
#include <string>
#include <sstream>

#include <base/basictypes.h>
#include <base/lazy_instance.h>
#include <base/logging.h>
#include <gtest/gtest_prod.h>

#include "shill/scope_logger.h"

// How to use:
//
// Use SMLOG and SMLOG_IF exactly as you would have used SLOG and SLOGIF.  I.e:
//
// SMLOG(WiFi, 2) << "My message at log verbosity level 2"
//                << " and yet more useful information";
//
// SMLOG_IF(WiMax, 3, some_var < 1024) << "If some_var is less than 1024, this"
//                                     << " message will be logged at level 3";
//
// This always puts the provided message into the MemoryLog instance, and will
// propagate the message through ScopeLogger into the VLOG system from
// base/logging.h if it meets appropriate scope/log level checks.
//
// Use MLOG and MLOG_IF exactly as you would have used LOG and LOG_IF.  I.e:
//
// MLOG(ERROR) << "Message logged at ERROR level";
//
// MLOG_IF(INFO, tacos < enough) << "Such a sad day";
//
// Once again, these messages will always go into the MemoryLog instance, and
// will be propagated down to the LOG system if they meet the severity checks in
// base/logging.h.
//
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


#define MLOG_SCOPE_STREAM(scope, verbose_level) \
    ::shill::MemoryLogMessage(__FILE__, __LINE__, \
                              -verbose_level, \
                              SLOG_IS_ON(scope, verbose_level)).stream()

#define MLOG_SEVERITY_STREAM(severity) \
    ::shill::MemoryLogMessage(__FILE__, __LINE__, \
                              logging::LOG_ ## severity, \
                              LOG_IS_ON(severity)).stream()

#define SMLOG(scope, verbose_level) MLOG_SCOPE_STREAM(scope, verbose_level)

#define SMLOG_IF(scope, verbose_level, condition) \
    LAZY_STREAM(MLOG_SCOPE_STREAM(scope, verbose_level), condition)

#define MLOG(severity) MLOG_SEVERITY_STREAM(severity)

#define MLOG_IF(severity, condition) \
    LAZY_STREAM(MLOG_SEVERITY_STREAM(severity), condition)

class MemoryLog {
 public:
  // Returns a singleton of this class.
  static MemoryLog *GetInstance();

  MemoryLog();
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
  FRIEND_TEST(MemoryLogTest, MemoryLogFlushToDiskWorks);
  FRIEND_TEST(MemoryLogTest, MemoryLogIsLogging);
  FRIEND_TEST(MemoryLogTest, MemoryLogLimitingWorks);

  // Arbitrary default verbose log capacity is an even megabyte.
  static const size_t kDefaultMaximumMemoryLogSizeInBytes = 1 << 20;

  std::deque<std::string> log_;

  void ShrinkToTargetSize(size_t number_bytes);
  size_t TestGetNumberMessages() { return log_.size(); }

  size_t maximum_size_bytes_;
  size_t current_size_bytes_;

  DISALLOW_COPY_AND_ASSIGN(MemoryLog);
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
  static const char kMemoryLogPrefix[];

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
