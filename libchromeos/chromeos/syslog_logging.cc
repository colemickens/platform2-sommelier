// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/syslog_logging.h>

#include <string>
#include <syslog.h>

// syslog.h and base/logging.h both try to #define LOG_INFO and LOG_WARNING.
// We need to #undef at least these two before including base/logging.h.  The
// others are included to be consistent.
namespace {
const int kSyslogInfo     = LOG_INFO;
const int kSyslogWarning  = LOG_WARNING;
const int kSyslogError    = LOG_ERR;
const int kSyslogCritical = LOG_CRIT;

#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERR
#undef LOG_INFO
}  // namespace

#include <base/logging.h>

static std::string s_ident;
static std::string s_accumulated;
static bool s_accumulate;
static bool s_log_to_syslog;
static bool s_log_to_stderr;

// TODO(cmasone): figure out if message_start can be used to help.
static bool HandleMessage(int severity,
                          const char* file,
                          int line,
                          size_t message_start,
                          const std::string &message) {
  switch (severity) {
    case logging::LOG_INFO:
      severity = kSyslogInfo;
      break;

    case logging::LOG_WARNING:
      severity = kSyslogWarning;
      break;

    case logging::LOG_ERROR:
    case logging::LOG_ERROR_REPORT:
      severity = kSyslogError;
      break;

    case logging::LOG_FATAL:
      severity = kSyslogCritical;
      break;
  }

  // The first "] " should be the end of the header added by the logging
  // code.  The meat of the message is two characters after that.
  size_t pos = message.find("] ");
  if (pos != std::string::npos && message.length() > pos + 2) {
    pos += 2;
  } else {
    pos = 0;
  }

  const char* str = message.c_str() + pos;

  if (s_log_to_syslog)
    syslog(severity, "%s", str);
  if (s_accumulate)
    s_accumulated.append(str);
  return !s_log_to_stderr;
}

namespace chromeos {
void InitLog(int init_flags) {
  logging::InitLogging("/dev/null",
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  logging::SetLogMessageHandler(HandleMessage);
  s_log_to_syslog = (init_flags & kLogToSyslog) != 0;
  s_log_to_stderr = (init_flags & kLogToStderr) != 0;
}
void OpenLog(const char* ident, bool log_pid) {
  s_ident = ident;
  openlog(s_ident.c_str(), log_pid ? LOG_PID : 0, LOG_USER);
}
void LogToString(bool enabled) {
  s_accumulate = enabled;
}
std::string GetLog() {
  return s_accumulated;
}
void ClearLog() {
  s_accumulated.clear();
}
bool FindLog(const char* string) {
  return s_accumulated.find(string) != std::string::npos;
}
}  // namespace chromeos
