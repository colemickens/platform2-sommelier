// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/arc_collector.h"

#include <sysexits.h>
#include <unistd.h>

#include <ctime>
#include <unordered_set>
#include <utility>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringize_macros.h>
#include <brillo/key_value_store.h>
#include <brillo/process.h>

using base::File;
using base::FilePath;
using base::ReadFileToString;

namespace {

const FilePath kContainerPidFile("/run/arc/container.pid");
const FilePath kArcRootPrefix("/opt/google/containers/android/rootfs/root");
const FilePath kArcBuildProp("system/build.prop");  // Relative to ARC root.

const char kCoreCollectorPath[] = "/usr/bin/core_collector";
const char kChromePath[] = "/opt/google/chrome/chrome";

const char kArcProduct[] = "ChromeOS_ARC";
const char kArcVersionField[] = "arc_version";

// Keys for crash log headers.
const char kActivityKey[] = "Activity";
const char kBuildKey[] = "Build";
const char kPackageKey[] = "Package";
const char kProcessKey[] = "Process";

const size_t kBufferSize = 4096;

inline bool IsAppProcess(const std::string &name) {
  return name == "app_process32" || name == "app_process64";
}

bool ReadCrashLogFromStdin(std::stringstream *stream);

bool HasExceptionInfo(const std::string &type);

bool GetChromeVersion(std::string *version);
bool GetArcVersion(std::string *version);

}  // namespace

ArcCollector::ArcCollector()
    : ArcCollector(ContextPtr(new ArcContext(this))) {
}

ArcCollector::ArcCollector(ContextPtr context)
    : UserCollectorBase("ARC"),
      context_(std::move(context)) {
}

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

bool ArcCollector::HandleJavaCrash(const std::string &type) {
  std::string reason;
  const bool should_dump = UserCollectorBase::ShouldDump(
      is_feedback_allowed_function_(), IsDeveloperImage(), &reason);

  std::ostringstream message;
  message << "Received " << type << " notification";

  if (!should_dump) {
    LogCrash(message.str(), reason);
    close(STDIN_FILENO);
    return true;
  }

  std::stringstream stream;
  if (!ReadCrashLogFromStdin(&stream)) {
    LOG(ERROR) << "Failed to read crash log";
    return false;
  }

  CrashLogHeaderMap map;
  std::string exception_info;
  if (!ParseCrashLog(type, &stream, &map, &exception_info)) {
    LOG(ERROR) << "Failed to parse crash log";
    return false;
  }

  const auto exec = GetCrashLogHeader(map, kProcessKey);
  message << " for " << exec;
  LogCrash(message.str(), reason);

  count_crash_function_();

  bool out_of_capacity = false;
  if (!CreateReportForJavaCrash(type, map, exception_info,
                                stream.str(), &out_of_capacity)) {
    if (!out_of_capacity)
      EnqueueCollectionErrorLog(0, kErrorSystemIssue, exec);

    return false;
  }

  return true;
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
  if (!IsArcProcess(pid)) {
    *reason = "ignoring - crash origin is not ARC";
    return false;
  }

  // TODO(domlaskowski): Convert between UID namespaces.
  if (uid >= kSystemUserEnd) {
    *reason = "ignoring - not a system process";
    return false;
  }

  return UserCollectorBase::ShouldDump(
      is_feedback_allowed_function_(), IsDeveloperImage(), reason);
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
    AddCrashMetaUploadData(kArcVersionField, version);
}

std::string ArcCollector::GetCrashLogHeader(const CrashLogHeaderMap &map,
                                            const char *key) {
  const auto it = map.find(key);
  return it == map.end() ? "unknown" : it->second;
}

bool ArcCollector::ParseCrashLog(const std::string &type,
                                 std::stringstream *stream,
                                 CrashLogHeaderMap *map,
                                 std::string *exception_info) {
  std::string line;

  // The last header is followed by an empty line.
  while (std::getline(*stream, line) && !line.empty()) {
    const auto end = line.find(':');

    if (end != std::string::npos) {
      const auto begin = line.find_first_not_of(' ', end + 1);

      if (begin != std::string::npos) {
        map->emplace(line.substr(0, end), line.substr(begin));
        continue;
      }
    }

    // Ignore malformed headers. The report is still created, but the associated
    // metadata fields are set to "unknown".
    LOG(WARNING) << "Header has unexpected format: " << line;
  }

  if (stream->fail())
    return false;

  if (HasExceptionInfo(type)) {
    std::ostringstream out;
    out << stream->rdbuf();
    *exception_info = out.str();
  }

  return true;
}

bool ArcCollector::CreateReportForJavaCrash(const std::string &type,
                                            const CrashLogHeaderMap &map,
                                            const std::string &exception_info,
                                            const std::string &log,
                                            bool *out_of_capacity) {
  FilePath crash_dir;
  if (!GetCreatedCrashDirectoryByEuid(geteuid(), &crash_dir, out_of_capacity)) {
    LOG(ERROR) << "Failed to create or find crash directory";
    return false;
  }

  const auto process = GetCrashLogHeader(map, kProcessKey);
  const auto basename = FormatDumpBasename(process, std::time(nullptr), 0);
  const FilePath log_path = GetCrashPath(crash_dir, basename, "log");

  const int size = static_cast<int>(log.size());
  if (WriteNewFile(log_path, log.c_str(), size) != size) {
    LOG(ERROR) << "Failed to write log";
    return false;
  }

  AddArcMetaData(type, false);
  AddCrashMetaUploadData(kArcVersionField, GetCrashLogHeader(map, kBuildKey));

  if (exception_info.empty()) {
    // Use the activity, package or process as the signature.
    std::string sig;
    if (map.count(kActivityKey)) {
      sig = GetCrashLogHeader(map, kActivityKey);
    } else if (map.count(kPackageKey)) {
      sig = GetCrashLogHeader(map, kPackageKey);
    } else {
      sig = process;
    }
    AddCrashMetaData("sig", sig);
  } else {
    const FilePath info_path = GetCrashPath(crash_dir, basename, "info");
    const int size = static_cast<int>(exception_info.size());

    if (WriteNewFile(info_path, exception_info.c_str(), size) != size) {
      LOG(ERROR) << "Failed to write exception info";
      return false;
    }

    AddCrashMetaUploadText("exception_info", info_path.value());
  }

  const FilePath meta_path = GetCrashPath(crash_dir, basename, "meta");
  WriteCrashMetaData(meta_path, process, log_path.value());
  return true;
}

namespace {

bool ReadCrashLogFromStdin(std::stringstream *stream) {
  File src(STDIN_FILENO);
  char buffer[kBufferSize];

  while (true) {
    const int count = src.ReadAtCurrentPosNoBestEffort(buffer, kBufferSize);
    if (count < 0)
      return false;

    if (count == 0)
      return stream->tellp() > 0;  // Crash log should not be empty.

    stream->write(buffer, count);
  }
}

bool HasExceptionInfo(const std::string &type) {
  static std::unordered_set<std::string> types = {
    "arc_app_crash",
    "arc_app_wtf",
    "arc_server_crash",
    "arc_server_wtf"
  };
  return types.count(type);
}

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
