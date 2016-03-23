// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/arc_collector.h"

#include <sysexits.h>
#include <unistd.h>

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringize_macros.h>
#include <brillo/key_value_store.h>
#include <brillo/process.h>

using base::FilePath;
using base::ReadFileToString;

namespace {

const FilePath kContainerPidFile("/run/arc/container.pid");
const FilePath kArcRootPrefix("/opt/google/containers/android/rootfs/root");
const FilePath kArcBuildProp("system/build.prop");  // Relative to ARC root.

const char kCoreCollectorPath[] = "/usr/bin/core_collector";
const char kChromePath[] = "/opt/google/chrome/chrome";

const char kArcProduct[] = "ChromeOS_ARC";

const size_t kBufferSize = 4096;

inline bool IsAppProcess(const std::string& name) {
  return name == "app_process32" || name == "app_process64";
}

bool GetChromeVersion(std::string *version);
bool GetArcVersion(std::string *version);

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
// TODO(domlaskowski): Dispatch to core_collector{32,64}.
#if __WORDSIZE == 64
  LOG(ERROR) << kCoreCollectorPath << "{32,64} not implemented";
  return kErrorUnsupported32BitCoreFile;
#else
  brillo::ProcessImpl core_collector;
  core_collector.AddArg(kCoreCollectorPath);
  core_collector.AddArg("--minidump=" + minidump_path.value());
  core_collector.AddArg("--coredump=" + core_path.value());
  core_collector.AddArg("--proc=" + container_dir.value());
  core_collector.AddArg("--prefix=" + kArcRootPrefix.value());
  core_collector.AddArg("--syslog");

  const int exit_code = core_collector.Run();
  if (exit_code == EX_OK) {
    AddArcMetaData("arc_native_crash", true);
    return kErrorNone;
  }

  LOG(ERROR) << kCoreCollectorPath << " failed with exit code " << exit_code;
  switch (exit_code) {
    case EX_OSFILE:
      return kErrorInvalidCoreFile;
    case EX_SOFTWARE:
      return kErrorCore2MinidumpConversion;
    default:
      return base::PathExists(core_path) ? kErrorSystemIssue :
                                           kErrorReadCoreData;
  }
#endif
}

void ArcCollector::AddArcMetaData(const std::string &type,
                                  bool add_arc_version) {
  AddCrashMetaUploadData("prod", kArcProduct);
  AddCrashMetaUploadData("type", type);

  std::string version;
  if (GetChromeVersion(&version))
    AddCrashMetaUploadData("chrome_version", version);

  if (add_arc_version && GetArcVersion(&version))
    AddCrashMetaUploadData("arc_version", version);
}

namespace {

bool GetChromeVersion(std::string *version) {
  brillo::ProcessImpl chrome;
  chrome.AddArg(kChromePath);
  chrome.AddArg("--product-version");
  chrome.RedirectUsingPipe(STDOUT_FILENO, false);
  if (chrome.Start()) {
    const int out = chrome.GetPipe(STDOUT_FILENO);
    char buffer[kBufferSize];
    version->clear();

    while (true) {
      const ssize_t count = HANDLE_EINTR(read(out, buffer, kBufferSize));
      if (count < 0)
        break;

      if (count == 0) {
        if (chrome.Wait() != EX_OK || version->empty())
          break;

        version->pop_back();  // Discard EOL.
        return true;
      }

      version->append(buffer, count);
    }
  }

  LOG(ERROR) << "Failed to get Chrome version";
  return false;
}

bool GetArcVersion(std::string *version) {
  brillo::KeyValueStore store;
  if (store.Load(kArcRootPrefix.Append(kArcBuildProp)) &&
      store.GetString("ro.build.fingerprint", version))
    return true;

  LOG(ERROR) << "Failed to get ARC version";
  return false;
}

}  // namespace
