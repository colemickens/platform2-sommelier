// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/user_collector_base.h"

#include <pcrecpp.h>

#if USE_DIRENCRYPTION
#include <keyutils.h>
#endif  // USE_DIRENCRYPTION

#include <base/files/file_util.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <brillo/syslog_logging.h>

#include "crash-reporter/util.h"

using base::FilePath;
using base::ReadFileToString;
using base::StringPrintf;

namespace {

const char kCollectionErrorSignature[] = "crash_reporter-user-collection";
const char kStatePrefix[] = "State:\t";
const char kUptimeField[] = "ptime";
const char kUserCrashSignal[] = "org.chromium.CrashReporter.UserCrash";

#if USE_DIRENCRYPTION
// Name of the session keyring.
const char kDircrypt[] = "dircrypt";
#endif  // USE_DIRENCRYPTION

void AccounceUserCrash() {
  std::string command = StringPrintf(
      "/usr/bin/dbus-send --type=signal --system / \"%s\" &", kUserCrashSignal);
  // Announce through D-Bus whenever a user crash happens. This is
  // used by the metrics daemon to log active use time between
  // crashes.
  //
  // This could be done more efficiently by explicit fork/exec or
  // using a dbus library directly. However, this should run
  // relatively rarely and longer term we may need to implement a
  // better way to do this that doesn't rely on D-Bus.
  //
  // We run in the background in case dbus daemon itself is crashed
  // and not responding.  This allows us to not block and potentially
  // deadlock on a dbus-daemon crash.  If dbus-daemon crashes without
  // restarting, each crash will fork off a lot of dbus-send
  // processes.  Such a system is in a unusable state and will need
  // to be restarted anyway.

  int status = system(command.c_str());
  LOG_IF(WARNING, status != 0) << "dbus-send running failed";
}

}  // namespace

const char* UserCollectorBase::kUserId = "Uid:\t";
const char* UserCollectorBase::kGroupId = "Gid:\t";

UserCollectorBase::UserCollectorBase(const std::string& collector_name,
                                     bool force_user_crash_dir)
    : CrashCollector(collector_name, force_user_crash_dir),
      tag_(collector_name) {}

void UserCollectorBase::Initialize(
    IsFeedbackAllowedFunction is_feedback_allowed_function,
    bool generate_diagnostics,
    bool directory_failure,
    const std::string& filter_in) {
  CrashCollector::Initialize(is_feedback_allowed_function);
  initialized_ = true;
  generate_diagnostics_ = generate_diagnostics;
  directory_failure_ = directory_failure;
  filter_in_ = filter_in;
}

bool UserCollectorBase::HandleCrash(const std::string& crash_attributes,
                                    const char* force_exec) {
  CHECK(initialized_);

  base::TimeDelta crash_time;
  GetUptime(&crash_time);

  pid_t pid;
  int signal;
  uid_t supplied_ruid;
  gid_t supplied_rgid;
  std::string kernel_supplied_name;

  if (!ParseCrashAttributes(crash_attributes, &pid, &signal, &supplied_ruid,
                            &supplied_rgid, &kernel_supplied_name)) {
    LOG(ERROR) << "Invalid parameter: --user=" << crash_attributes;
    return false;
  }

  std::string exec;
  if (force_exec) {
    exec.assign(force_exec);
  } else if (!GetExecutableBaseNameFromPid(pid, &exec)) {
    // If we cannot find the exec name, use the kernel supplied name.
    // We don't always use the kernel's since it truncates the name to
    // 16 characters.
    exec = StringPrintf("supplied_%s", kernel_supplied_name.c_str());
  }

  // Allow us to test the crash reporting mechanism successfully even if
  // other parts of the system crash.
  if (!filter_in_.empty() && (filter_in_ == "none" || filter_in_ != exec)) {
    // We use a different format message to make it more obvious in tests
    // which crashes are test generated and which are real.
    LOG(WARNING) << "Ignoring crash from " << exec << "[" << pid << "] while "
                 << "filter_in=" << filter_in_ << ".";
    return true;
  }

  std::string reason;
  bool dump = ShouldDump(pid, supplied_ruid, exec, &reason);

  const auto message = StringPrintf(
      "Received crash notification for %s[%d] sig %d, user %u group %u",
      exec.c_str(), pid, signal, supplied_ruid, supplied_rgid);

  LogCrash(message, reason);

  if (dump) {
    AccounceUserCrash();

    AddExtraMetadata(exec, pid);

    if (generate_diagnostics_) {
      bool out_of_capacity = false;
      ErrorType error_type =
          ConvertAndEnqueueCrash(pid, exec, supplied_ruid, supplied_rgid,
                                 crash_time, &out_of_capacity);
      if (error_type != kErrorNone) {
        if (!out_of_capacity)
          EnqueueCollectionErrorLog(pid, error_type, exec);
        return false;
      }
    }
  }

  return true;
}

bool UserCollectorBase::ParseCrashAttributes(
    const std::string& crash_attributes,
    pid_t* pid,
    int* signal,
    uid_t* uid,
    gid_t* gid,
    std::string* exec_name) {
  pcrecpp::RE re("(\\d+):(\\d+):(\\d+):(\\d+):(.*)");
  return re.FullMatch(crash_attributes, pid, signal, uid, gid, exec_name);
}

bool UserCollectorBase::ShouldDump(bool has_owner_consent,
                                   bool is_developer,
                                   std::string* reason) const {
  // For developer builds, we always want to keep the crash reports unless
  // we're testing the crash facilities themselves.  This overrides
  // feedback.  Crash sending still obeys consent.
  if (is_developer) {
    *reason = "developer build - not testing - always dumping";
    return true;
  }

  if (!has_owner_consent) {
    *reason = "ignoring - no consent";
    return false;
  }

  *reason = "handling";
  return true;
}

void UserCollectorBase::LogCrash(const std::string& message,
                                 const std::string& reason) const {
  LOG(WARNING) << '[' << tag_ << "] " << message << " (" << reason << ')';
}

bool UserCollectorBase::GetFirstLineWithPrefix(
    const std::vector<std::string>& lines,
    const char* prefix,
    std::string* line) {
  for (const auto& current_line : lines) {
    if (current_line.find(prefix) == 0) {
      *line = current_line;
      return true;
    }
  }
  return false;
}

bool UserCollectorBase::GetIdFromStatus(
    const char* prefix,
    IdKind kind,
    const std::vector<std::string>& status_lines,
    int* id) {
  // From fs/proc/array.c:task_state(), this file contains:
  // \nUid:\t<uid>\t<euid>\t<suid>\t<fsuid>\n
  std::string id_line;
  if (!GetFirstLineWithPrefix(status_lines, prefix, &id_line)) {
    return false;
  }
  std::string id_substring = id_line.substr(strlen(prefix), std::string::npos);
  std::vector<std::string> ids = base::SplitString(
      id_substring, "\t", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (ids.size() != kIdMax || kind < 0 || kind >= kIdMax) {
    return false;
  }
  const char* number = ids[kind].c_str();
  char* end_number = nullptr;
  *id = strtol(number, &end_number, 10);
  if (*end_number != '\0') {
    return false;
  }
  return true;
}

bool UserCollectorBase::GetStateFromStatus(
    const std::vector<std::string>& status_lines, std::string* state) {
  std::string state_line;
  if (!GetFirstLineWithPrefix(status_lines, kStatePrefix, &state_line)) {
    return false;
  }
  *state = state_line.substr(strlen(kStatePrefix), std::string::npos);
  return true;
}

bool UserCollectorBase::ClobberContainerDirectory(
    const base::FilePath& container_dir) {
  // Delete a pre-existing directory from crash reporter that may have
  // been left around for diagnostics from a failed conversion attempt.
  // If we don't, existing files can cause forking to fail.
  if (!base::DeleteFile(container_dir, true)) {
    PLOG(ERROR) << "Could not delete " << container_dir.value();
    return false;
  }

  if (!base::CreateDirectory(container_dir)) {
    PLOG(ERROR) << "Could not create " << container_dir.value();
    return false;
  }

  return true;
}

const FilePath UserCollectorBase::GetCrashProcessingDir() {
  return FilePath("/tmp/crash_reporter");
}

UserCollectorBase::ErrorType UserCollectorBase::ConvertAndEnqueueCrash(
    pid_t pid,
    const std::string& exec,
    uid_t supplied_ruid,
    gid_t supplied_rgid,
    const base::TimeDelta& crash_time,
    bool* out_of_capacity) {
  FilePath crash_path;
  if (!GetCreatedCrashDirectory(pid, supplied_ruid, &crash_path,
                                out_of_capacity)) {
    LOG(ERROR) << "Unable to find/create process-specific crash path";
    return kErrorSystemIssue;
  }

  // Directory like /tmp/crash_reporter/1234 which contains the
  // procfs entries and other temporary files used during conversion.
  const FilePath container_dir =
      GetCrashProcessingDir().Append(StringPrintf("%d", pid));
  if (!ClobberContainerDirectory(container_dir))
    return kErrorSystemIssue;

  std::string dump_basename = FormatDumpBasename(exec, time(nullptr), pid);
  FilePath core_path = GetCrashPath(crash_path, dump_basename, "core");
  FilePath meta_path = GetCrashPath(crash_path, dump_basename, "meta");
  FilePath minidump_path = GetCrashPath(crash_path, dump_basename, "dmp");
  FilePath log_path = GetCrashPath(crash_path, dump_basename, "log");
  FilePath proc_log_path = GetCrashPath(crash_path, dump_basename, "proclog");

  if (GetLogContents(FilePath(log_config_path_), exec, log_path)) {
    FilePath full_path = log_path;
    if (!NormalizeFilePath(log_path, &full_path))
      full_path = log_path;
    AddCrashMetaUploadFile("log", full_path.value());
  }

  if (GetProcessTree(pid, proc_log_path)) {
    FilePath full_path = proc_log_path;
    if (!NormalizeFilePath(proc_log_path, &full_path))
      full_path = proc_log_path;
    AddCrashMetaUploadFile("process_tree", full_path.value());
  }

#if USE_DIRENCRYPTION
  // Join the session keyring, if one exists.
  JoinSessionKeyring();
#endif  // USE_DIRENCRYPTION

  ErrorType error_type =
      ConvertCoreToMinidump(pid, container_dir, core_path, minidump_path);
  if (error_type != kErrorNone) {
    if (error_type != kErrorReadCoreData)
      LOG(INFO) << "Leaving core file at " << core_path.value()
                << " due to conversion error";
    return error_type;
  } else {
    base::FilePath target;
    if (!NormalizeFilePath(minidump_path, &target))
      target = minidump_path;
    LOG(INFO) << "Stored minidump to " << target.value();
  }

  base::TimeDelta start_time;
  if (GetUptimeAtProcessStart(pid, &start_time) && crash_time > start_time) {
    const base::TimeDelta uptime = crash_time - start_time;
    AddCrashMetaUploadData(kUptimeField,
                           std::to_string(uptime.InMilliseconds()));
  } else {
    LOG(WARNING) << "Failed to get process uptime.";
  }

  // Here we commit to sending this file.  We must not return false
  // after this point or we will generate a log report as well as a
  // crash report.
  WriteCrashMetaData(meta_path, exec, minidump_path.value());

  if (!util::IsDeveloperImage()) {
    base::DeleteFile(core_path, false);
  } else {
    LOG(INFO) << "Leaving core file at " << core_path.value()
              << " due to developer image";
  }

  base::DeleteFile(container_dir, true);
  return kErrorNone;
}

std::string UserCollectorBase::GetErrorTypeSignature(
    ErrorType error_type) const {
  switch (error_type) {
    case kErrorSystemIssue:
      return "system-issue";
    case kErrorReadCoreData:
      return "read-core-data";
    case kErrorUnusableProcFiles:
      return "unusable-proc-files";
    case kErrorInvalidCoreFile:
      return "invalid-core-file";
    case kErrorUnsupported32BitCoreFile:
      return "unsupported-32bit-core-file";
    case kErrorCore2MinidumpConversion:
      return "core2md-conversion";
    default:
      return "";
  }
}

bool UserCollectorBase::GetCreatedCrashDirectory(pid_t pid,
                                                 uid_t supplied_ruid,
                                                 FilePath* crash_file_path,
                                                 bool* out_of_capacity) {
  FilePath process_path = GetProcessPath(pid);
  std::string status;
  if (directory_failure_) {
    LOG(ERROR) << "Purposefully failing to create spool directory";
    return false;
  }

  uid_t uid;
  if (base::ReadFileToString(process_path.Append("status"), &status)) {
    std::vector<std::string> status_lines = base::SplitString(
        status, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

    std::string process_state;
    if (!GetStateFromStatus(status_lines, &process_state)) {
      LOG(ERROR) << "Could not find process state in status file";
      return false;
    }
    LOG(INFO) << "State of crashed process [" << pid << "]: " << process_state;

    // Get effective UID of crashing process.
    int id;
    if (!GetIdFromStatus(kUserId, kIdEffective, status_lines, &id)) {
      LOG(ERROR) << "Could not find euid in status file";
      return false;
    }
    uid = id;
  } else {
    LOG(INFO) << "Using supplied UID " << supplied_ruid
              << " for crashed process [" << pid
              << "] due to error reading status file";
    uid = supplied_ruid;
  }

  if (!GetCreatedCrashDirectoryByEuid(uid, crash_file_path, out_of_capacity)) {
    LOG(ERROR) << "Could not create crash directory";
    return false;
  }
  return true;
}

void UserCollectorBase::EnqueueCollectionErrorLog(pid_t pid,
                                                  ErrorType error_type,
                                                  const std::string& exec) {
  FilePath crash_path;
  LOG(INFO) << "Writing conversion problems as separate crash report.";
  if (!GetCreatedCrashDirectoryByEuid(0, &crash_path, nullptr)) {
    LOG(ERROR) << "Could not even get log directory; out of space?";
    return;
  }
  AddCrashMetaData("sig", kCollectionErrorSignature);
  AddCrashMetaData("error_type", GetErrorTypeSignature(error_type));
  std::string dump_basename = FormatDumpBasename(exec, time(nullptr), pid);
  std::string error_log = brillo::GetLog();
  FilePath diag_log_path = GetCrashPath(crash_path, dump_basename, "diaglog");
  if (GetLogContents(FilePath(log_config_path_), kCollectionErrorSignature,
                     diag_log_path)) {
    // We load the contents of diag_log into memory and append it to
    // the error log.  We cannot just append to files because we need
    // to always create new files to prevent attack.
    std::string diag_log_contents;
    base::ReadFileToString(diag_log_path, &diag_log_contents);
    error_log.append(diag_log_contents);
    base::DeleteFile(diag_log_path, false);
  }
  FilePath log_path = GetCrashPath(crash_path, dump_basename, "log");
  FilePath meta_path = GetCrashPath(crash_path, dump_basename, "meta");
  // We must use WriteNewFile instead of base::WriteFile as we do
  // not want to write with root access to a symlink that an attacker
  // might have created.
  if (WriteNewFile(log_path, error_log.data(), error_log.length()) < 0) {
    PLOG(ERROR) << "Error writing new file " << log_path.value();
    return;
  }
  WriteCrashMetaData(meta_path, exec, log_path.value());
}

std::vector<std::string> UserCollectorBase::GetCommandLine(pid_t pid) const {
  const FilePath path = GetProcessPath(pid).Append("cmdline");
  // The /proc/[pid]/cmdline file contains the command line separated and
  // terminated by a null byte, e.g. "command\0arg\0arg\0". The file is
  // empty if the process is a zombie.
  std::string cmdline;
  if (!ReadFileToString(path, &cmdline)) {
    PLOG(ERROR) << "Could not read " << path.value();
    return std::vector<std::string>();
  }

  if (cmdline.empty()) {
    LOG(ERROR) << "Empty cmdline for " << path.value();
    return std::vector<std::string>();
  }

  // Split the string by null bytes.
  return base::SplitString(cmdline, std::string(1, '\0'), base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

#if USE_DIRENCRYPTION
void UserCollectorBase::JoinSessionKeyring() {
  key_serial_t session_keyring = keyctl_join_session_keyring(kDircrypt);
  if (session_keyring == -1) {
    // The session keyring may not exist if ext4 encryption isn't enabled so
    // just log an info message instead of an error.
    PLOG(INFO) << "Unable to join session keying";
  }
}
#endif  // USE_DIRENCRYPTION
