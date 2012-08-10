// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_log.h"

#include <string>

#include "shill/memory_log.h"

using std::string;

namespace shill {

ScopedMockLog *ScopedMockLog::instance_ = NULL;

ScopedMockLog::ScopedMockLog() {
  previous_handler_ = ::logging::GetLogMessageHandler();
  ::logging::SetLogMessageHandler(HandleLogMessages);
  instance_ = this;
}

ScopedMockLog::~ScopedMockLog() {
  ::logging::SetLogMessageHandler(previous_handler_);
  instance_ = NULL;
}

// static
bool ScopedMockLog::HandleLogMessages(int severity,
                                      const char *file,
                                      int line,
                                      size_t message_start,
                                      const string &full_message) {
  CHECK(instance_);

  // |full_message| looks like this if it came through MemoryLog:
  //   "[0514/165501:INFO:mock_log_unittest.cc(22)] memlog: Some message\n"
  // The user wants to match just the substring "Some message".  Strip off the
  // extra stuff.  |message_start| is the position where "memlog: " begins.
  //
  // However, if the message did not come through MemoryLog, then it is missing
  // the "memlog: " prefix.  Try and detect this situation and trim off the
  // prefix when present.
  size_t prefix_length = strlen(MemoryLog::kMemoryLogPrefix);
  if (strncmp(full_message.c_str() + message_start,
              MemoryLog::kMemoryLogPrefix,
              prefix_length)) {
    // No prefix to trim
    prefix_length = 0;
  }
  // Note that the -1 is to remove the trailing return line.
  const string::size_type message_length =
      full_message.length() - message_start - 1 - prefix_length;
  const string message(full_message,
                       message_start + prefix_length,
                       message_length);

  // Call Log.  Because Log is a mock method, this sets in motion the mocking
  // magic.
  instance_->Log(severity, file, message);

  // Invoke the previously installed message handler if there was one.
  if (instance_->previous_handler_ != NULL) {
    return (*instance_->previous_handler_)(severity, file, line,
                                           message_start, full_message);
  }

  // Return false so that messages show up on stderr.
  return false;
}

}  // namespace shill
