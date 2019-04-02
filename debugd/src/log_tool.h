// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_LOG_TOOL_H_
#define DEBUGD_SRC_LOG_TOOL_H_

#include <sys/types.h>
#include <map>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

#include "debugd/src/anonymizer_tool.h"
#include "debugd/src/sandboxed_process.h"

namespace debugd {

class LogTool {
 public:
  // The encoding for a particular log.
  enum class Encoding {
    // Tries to see if the log output is valid UTF-8. Outputs it as-is if it is,
    // or base64-encodes it otherwise.
    kAutodetect,

    // Replaces any characters that are not valid UTF-8 encoded with the
    // replacement character.
    kUtf8,

    // base64-encodes the output.
    kBinary
  };

  class Log {
   public:
    enum LogType { kCommand, kFile };

    static constexpr int64_t kDefaultMaxBytes = 512 * 1024;

    Log(LogType type,
        std::string name,
        std::string data,
        std::string user = SandboxedProcess::kDefaultUser,
        std::string group = SandboxedProcess::kDefaultGroup,
        int64_t max_bytes = kDefaultMaxBytes,
        LogTool::Encoding encoding = LogTool::Encoding::kAutodetect);

    std::string GetName() const;
    std::string GetLogData() const;

    std::string GetCommandLogData() const;
    std::string GetFileLogData() const;

    void DisableMinijailForTest();

   private:
    static uid_t UidForUser(const std::string& name);
    static gid_t GidForGroup(const std::string& group);

    LogType type_;
    std::string name_;
    // For kCommand logs, this is the command to run.
    // For kFile logs, this is the file path to read.
    std::string data_;
    std::string user_;
    std::string group_;
    int64_t max_bytes_;  // passed as arg to 'tail -c'
    LogTool::Encoding encoding_;

    bool minijail_disabled_for_test_ = false;
  };

  explicit LogTool(scoped_refptr<dbus::Bus> bus) : bus_(bus) {}
  ~LogTool() = default;

  using LogMap = std::map<std::string, std::string>;

  std::string GetLog(const std::string& name);
  LogMap GetAllLogs();
  LogMap GetAllDebugLogs();
  LogMap GetFeedbackLogs();
  void GetBigFeedbackLogs(const base::ScopedFD& fd);
  LogMap GetUserLogFiles();

  // Returns a representation of |value| that is valid UTF-8 encoded. The value
  // of |source_encoding| determines whether it will use Unicode U+FFFD
  // REPLACEMENT CHARACTER or base64-encode the whole string in case there are
  // invalid characters.
  static std::string EnsureUTF8String(std::string value,
                                      Encoding source_encoding);

 private:
  friend class LogToolTest;

  void AnonymizeLogMap(LogMap* log_map);
  void CreateConnectivityReport(bool wait_for_results);

  scoped_refptr<dbus::Bus> bus_;

  AnonymizerTool anonymizer_;

  DISALLOW_COPY_AND_ASSIGN(LogTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_LOG_TOOL_H_
