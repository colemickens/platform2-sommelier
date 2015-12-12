// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/arc_collector.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringize_macros.h>

using base::FilePath;
using base::ReadFileToString;

namespace {

const FilePath kContainerPidFile("/run/arc/container.pid");

inline bool IsAppProcess(const std::string& name) {
  return name == "app_process32" || name == "app_process64";
}

}  // namespace

ArcCollector::ArcCollector(ContextPtr context)
    : context_(std::move(context)) {}

bool ArcCollector::IsArcProcess(pid_t pid) const {
  pid_t arc_pid;
  if (!context_->GetArcPid(&arc_pid)) {
    LOG(ERROR) << "Failed to get PID of ARC container";
    return false;
  }
  std::string arc_ns;
  if (!context_->GetPidNamespace(arc_pid, &arc_ns)) {
    LOG(ERROR) << "Failed to get PID namespace of ARC container";
    return false;
  }
  std::string ns;
  if (!context_->GetPidNamespace(pid, &ns)) {
    LOG(ERROR) << "Failed to get PID namespace of process";
    return false;
  }
  return ns == arc_ns;
}

bool ArcCollector::IsArcRunning() {
  return base::PathExists(kContainerPidFile);
}

bool ArcCollector::ArcContext::GetArcPid(pid_t *pid) const {
  std::string buf;
  return ReadFileToString(kContainerPidFile, &buf) && base::StringToInt(
      base::StringPiece(buf.begin(), buf.end() - 1), pid);  // Trim EOL.
}

bool ArcCollector::ArcContext::GetPidNamespace(pid_t pid,
                                               std::string *ns) const {
  const FilePath path =
      collector_->GetProcessPath(pid).Append("ns").Append("pid");

  // The /proc/[pid]/ns/pid file is a special symlink that resolves to a string
  // containing the inode number of the PID namespace, e.g. "pid:[4026531838]".
  FilePath target;
  if (!collector_->GetSymlinkTarget(path, &target))
    return false;

  *ns = target.value();
  return true;
}

bool ArcCollector::ArcContext::GetExeBaseName(pid_t pid,
                                              std::string *exe) const {
  return collector_->CrashCollector::GetExecutableBaseNameFromPid(pid, exe);
}

bool ArcCollector::ArcContext::GetCommand(pid_t pid,
                                          std::string *command) const {
  const FilePath path = collector_->GetProcessPath(pid).Append("cmdline");
  // The /proc/[pid]/cmdline file contains the command line separated and
  // terminated by a null byte, e.g. "command\0arg\0arg\0". The file is
  // empty if the process is a zombie.
  if (!ReadFileToString(path, command))
    return false;
  const auto pos = command->find('\0');
  if (pos == std::string::npos)
    return false;
  command->erase(pos);  // Discard command-line arguments.
  return true;
}

bool ArcCollector::GetExecutableBaseNameFromPid(pid_t pid,
                                                std::string *base_name) {
  if (!context_->GetExeBaseName(pid, base_name))
    return false;

  // The runtime for non-native ARC apps overwrites its command line with the
  // package name of the app, so use that instead.
  if (IsArcProcess(pid) && IsAppProcess(*base_name)) {
    if (!context_->GetCommand(pid, base_name))
      LOG(ERROR) << "Failed to get package name";
  }
  return true;
}

bool ArcCollector::ShouldDump(pid_t pid,
                              uid_t uid,
                              const std::string &exec,
                              std::string *reason) {
  *reason = "ARC: ";

  if (!IsArcProcess(pid)) {
    reason->append("ignoring - crash origin is not ARC");
    return false;
  }

  // TODO(domlaskowski): Convert between UID namespaces.
  if (uid != kSystemUser) {
    reason->append("ignoring - not a system process");
    return false;
  }

  std::string message;
  const bool dump = UserCollectorBase::ShouldDump(
      is_feedback_allowed_function_(), IsDeveloperImage(), &message);

  reason->append(message);
  return dump;
}

UserCollectorBase::ErrorType ArcCollector::ConvertCoreToMinidump(
    pid_t pid,
    const base::FilePath &container_dir,
    const base::FilePath &core_path,
    const base::FilePath &minidump_path) {
  // TODO(domlaskowski): Invoke arc_collector.
  LOG(ERROR) << "ARC collector not implemented";
  return kErrorCore2MinidumpConversion;
}
