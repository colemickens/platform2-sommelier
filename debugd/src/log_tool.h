// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_LOG_TOOL_H_
#define DEBUGD_SRC_LOG_TOOL_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <dbus/bus.h>
#include <dbus/file_descriptor.h>

#include "debugd/src/anonymizer_tool.h"

namespace debugd {

class LogTool {
 public:
  LogTool() = default;
  ~LogTool() = default;

  using LogMap = std::map<std::string, std::string>;

  std::string GetLog(const std::string& name);
  LogMap GetAllLogs(scoped_refptr<dbus::Bus> bus);
  LogMap GetFeedbackLogs(scoped_refptr<dbus::Bus> bus);
  void GetBigFeedbackLogs(scoped_refptr<dbus::Bus> bus,
                          const dbus::FileDescriptor& fd);
  LogMap GetUserLogFiles();

 private:
  friend class LogToolTest;

  void AnonymizeLogMap(LogMap* log_map);
  void CreateConnectivityReport(scoped_refptr<dbus::Bus> bus);

  AnonymizerTool anonymizer_;

  DISALLOW_COPY_AND_ASSIGN(LogTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_LOG_TOOL_H_
