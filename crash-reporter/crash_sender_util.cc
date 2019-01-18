// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/crash_sender_util.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>

#include "crash-reporter/crash_sender_paths.h"
#include "crash-reporter/paths.h"
#include "crash-reporter/util.h"

namespace util {

namespace {

// getenv() wrapper that returns an empty string, if the environment variable is
// not defined.
std::string GetEnv(const std::string& name) {
  const char* value = getenv(name.c_str());
  return value ? value : "";
}

// Shows the usage of crash_sender and exits the process as a success.
void ShowUsageAndExit() {
  printf(
      "Usage: crash_sender [options]\n"
      "Options:\n"
      " -e <var>=<val>     Set env |var| to |val| (only some vars)\n");
  exit(EXIT_SUCCESS);
}


// Returns true if the given report kind is known.
// TODO(satorux): Move collector constants to a common file.
bool IsKnownKind(const std::string& kind) {
  return (kind == "minidump" || kind == "kcrash" || kind == "log" ||
          kind == "devcore" || kind == "eccrash" || kind == "bertdump");
}

// Returns true if the given key is valid for crash metadata.
bool IsValidKey(const std::string& key) {
  if (key.empty())
    return false;

  for (const char c : key) {
    if (!(base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '_' ||
          c == '-' || c == '.')) {
      return false;
    }
  }

  return true;
}

}  // namespace

void ParseCommandLine(int argc,
                      const char* const* argv,
                      CommandLineFlags* flags) {
  std::map<std::string, std::string> env_vars;
  for (const EnvPair& pair : kEnvironmentVariables) {
    // Honor the existing value if it's already set.
    const char* value = getenv(pair.name);
    env_vars[pair.name] = value ? value : pair.value;
  }

  // Process -e options, and collect other options.
  std::vector<const char*> new_argv;
  new_argv.push_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "-e") {
      if (i + 1 < argc) {
        ++i;
        std::string name_value = argv[i];
        std::vector<std::string> pair = base::SplitString(
            name_value, "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
        if (pair.size() == 2) {
          if (env_vars.count(pair[0]) == 0) {
            LOG(ERROR) << "Unknown variable name: " << pair[0];
            exit(EXIT_FAILURE);
          }
          env_vars[pair[0]] = pair[1];
        } else {
          LOG(ERROR) << "Malformed value for -e: " << name_value;
          exit(EXIT_FAILURE);
        }
      } else {
        LOG(ERROR) << "Value for -e is missing";
        exit(EXIT_FAILURE);
      }
    } else {
      new_argv.push_back(argv[i]);
    }
  }
  // argv[argc] should be a null pointer per the C standard.
  new_argv.push_back(nullptr);

  // Process the remaining flags.
  DEFINE_bool(h, false, "Show this help and exit");
  DEFINE_int32(max_spread_time, kMaxSpreadTimeInSeconds,
               "Max time in secs to sleep before sending (0 to send now)");
  brillo::FlagHelper::Init(new_argv.size() - 1, new_argv.data(),
                           "Chromium OS Crash Sender");
  // TODO(satorux): Remove this once -e option is gone.
  if (FLAGS_h)
    ShowUsageAndExit();

  if (FLAGS_max_spread_time < 0) {
    LOG(ERROR) << "Invalid value for max spread time: "
               << FLAGS_max_spread_time;
    exit(EXIT_FAILURE);
  }
  flags->max_spread_time = base::TimeDelta::FromSeconds(FLAGS_max_spread_time);

  // Set the predefined environment variables.
  for (const auto& it : env_vars)
    setenv(it.first.c_str(), it.second.c_str(), 1 /* overwrite */);
}

bool IsMock() {
  return base::PathExists(
      paths::GetAt(paths::kSystemRunStateDirectory, paths::kMockCrashSending));
}

bool ShouldPauseSending() {
  return (base::PathExists(paths::Get(paths::kPauseCrashSending)) &&
          GetEnv("OVERRIDE_PAUSE_SENDING") == "0");
}

bool CheckDependencies(base::FilePath* missing_path) {
  const char* const kDependencies[] = {
      paths::kFind, paths::kMetricsClient,
      paths::kRestrictedCertificatesDirectory,
  };

  for (const char* dependency : kDependencies) {
    const base::FilePath path = paths::Get(dependency);
    int permissions = 0;
    // Check if |path| is an executable or a directory.
    if (!(base::GetPosixFilePermissions(path, &permissions) &&
          (permissions & base::FILE_PERMISSION_EXECUTE_BY_USER))) {
      *missing_path = path;
      return false;
    }
  }
  return true;
}

base::FilePath GetBasePartOfCrashFile(const base::FilePath& file_name) {
  std::vector<std::string> components;
  file_name.GetComponents(&components);

  std::vector<std::string> parts = base::SplitString(
      components.back(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() < 4) {
    LOG(ERROR) << "Unexpected file name format: " << file_name.value();
    return file_name;
  }

  parts.resize(4);
  const std::string base_name = base::JoinString(parts, ".");

  if (components.size() == 1)
    return base::FilePath(base_name);
  return file_name.DirName().Append(base_name);
}

void RemoveOrphanedCrashFiles(const base::FilePath& crash_dir) {
  base::FileEnumerator iter(crash_dir, true /* recursive */,
                            base::FileEnumerator::FILES, "*");
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    // Get the meta data file path.
    const base::FilePath meta_file =
        base::FilePath(GetBasePartOfCrashFile(file).value() + ".meta");

    // Check how old the file is.
    base::File::Info info;
    if (!base::GetFileInfo(file, &info)) {
      LOG(WARNING) << "Failed to get file info: " << file.value();
      continue;
    }
    base::TimeDelta delta = base::Time::Now() - info.last_modified;

    if (!base::PathExists(meta_file) && delta.InHours() >= 24) {
      LOG(INFO) << "Removing old orphaned file: " << file.value();
      if (!base::DeleteFile(file, false /* recursive */))
        PLOG(WARNING) << "Failed to remove " << file.value();
    }
  }
}

Action ChooseAction(const base::FilePath& meta_file,
                    MetricsLibraryInterface* metrics_lib,
                    std::string* reason) {
  if (!IsMock() && !IsOfficialImage()) {
    *reason = "Not an official OS version";
    return kRemove;
  }

  // AreMetricsEnabled() returns false in guest mode, thus IsGuestMode() should
  // also be checked here (otherwise, all crash files are deleted in guest
  // mode).
  //
  // Note that this check is slightly racey, but should be rare enough for us
  // not to care:
  //
  // - crash_sender checks IsGuestMode() and it returns false
  // - User logs in to guest mode
  // - crash_sender checks AreMetricsEnabled() and it's now false
  // - Reports are deleted
  if (!metrics_lib->IsGuestMode() && !metrics_lib->AreMetricsEnabled()) {
    *reason = "Crash reporting is disabled";
    return kRemove;
  }

  std::string raw_metadata;
  if (!base::ReadFileToString(meta_file, &raw_metadata)) {
    PLOG(WARNING) << "Igonoring: metadata file is inaccessible";
    return kIgnore;
  }

  brillo::KeyValueStore metadata;
  if (!ParseMetadata(raw_metadata, &metadata)) {
    *reason = "Corrupted metadata: " + raw_metadata;
    return kRemove;
  }

  base::FilePath payload_path = GetBaseNameFromMetadata(metadata, "payload");
  if (payload_path.empty()) {
    *reason = "Payload is not found in the meta data: " + raw_metadata;
    return kRemove;
  }

  // Make it an absolute path.
  payload_path = meta_file.DirName().Append(payload_path);

  if (!base::PathExists(payload_path)) {
    // TODO(satorux): logging_CrashSender.py expects "Missing payload" in the
    // error message. Revise the autotest once the rewrite to C++ is complete.
    *reason = "Missing payload: " + payload_path.value();
    return kRemove;
  }

  const std::string kind = GetKindFromPayloadPath(payload_path);
  if (!IsKnownKind(kind)) {
    *reason = "Unknown kind: " + kind;
    return kRemove;
  }

  if (!IsCompleteMetadata(metadata)) {
    base::File::Info info;
    if (!base::GetFileInfo(meta_file, &info)) {
      // Should not happen since it succeeded to read the file.
      *reason = "Failed to get file info";
      return kIgnore;
    }

    const base::TimeDelta delta = base::Time::Now() - info.last_modified;
    if (delta.InHours() >= 24) {
      // TODO(satorux): logging_CrashSender.py expects the following string as
      // error message. Revise the autotest once the rewrite to C++ is complete.
      *reason = "Removing old incomplete metadata";
      return kRemove;
    } else {
      *reason = "Recent incomplete metadata";
      return kIgnore;
    }
  }

  if (kind == "devcore" && !IsDeviceCoredumpUploadAllowed()) {
    *reason = "Device coredump upload not allowed";
    return kIgnore;
  }

  return kSend;
}

void RemoveAndPickCrashFiles(const base::FilePath& crash_dir,
                             MetricsLibraryInterface* metrics_lib,
                             std::vector<base::FilePath>* to_send) {
  std::vector<base::FilePath> meta_files = GetMetaFiles(crash_dir);

  for (const auto& meta_file : meta_files) {
    LOG(INFO) << "Checking metadata: " << meta_file.value();

    std::string reason;
    switch (ChooseAction(meta_file, metrics_lib, &reason)) {
      case kRemove:
        LOG(INFO) << "Removing: " << reason;
        RemoveReportFiles(meta_file);
        break;
      case kIgnore:
        LOG(INFO) << "Igonoring: " << reason;
        break;
      case kSend:
        to_send->push_back(meta_file);
        break;
      default:
        NOTREACHED();
    }
  }
}

void RemoveReportFiles(const base::FilePath& meta_file) {
  if (meta_file.Extension() != ".meta") {
    LOG(ERROR) << "Not a meta file: " << meta_file.value();
    return;
  }

  const std::string pattern =
      meta_file.BaseName().RemoveExtension().value() + ".*";

  base::FileEnumerator iter(meta_file.DirName(), false /* recursive */,
                            base::FileEnumerator::FILES, pattern);
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    if (!base::DeleteFile(file, false /* recursive */))
      PLOG(WARNING) << "Failed to remove " << file.value();
  }
}

std::vector<base::FilePath> GetMetaFiles(const base::FilePath& crash_dir) {
  base::FileEnumerator iter(crash_dir, false /* recursive */,
                            base::FileEnumerator::FILES, "*.meta");
  std::vector<std::pair<base::Time, base::FilePath>> time_meta_pairs;
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    base::File::Info info;
    if (!base::GetFileInfo(file, &info)) {
      PLOG(WARNING) << "Failed to get file info: " << file.value();
      continue;
    }
    time_meta_pairs.push_back(std::make_pair(info.last_modified, file));
  }
  std::sort(time_meta_pairs.begin(), time_meta_pairs.end());

  std::vector<base::FilePath> meta_files;
  for (const auto& pair : time_meta_pairs)
    meta_files.push_back(pair.second);
  return meta_files;
}

base::FilePath GetBaseNameFromMetadata(const brillo::KeyValueStore& metadata,
                                       const std::string& key) {
  std::string value;
  if (!metadata.GetString(key, &value))
    return base::FilePath();

  return base::FilePath(value).BaseName();
}

std::string GetKindFromPayloadPath(const base::FilePath& payload_path) {
  std::vector<std::string> parts =
      base::SplitString(payload_path.BaseName().value(), ".",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // Suppress "gz".
  if (parts.size() >= 2 && parts.back() == "gz")
    parts.pop_back();

  if (parts.size() <= 1)
    return "";

  std::string extension = parts.back();
  if (extension == "dmp")
    return "minidump";

  return extension;
}

bool ParseMetadata(const std::string& raw_metadata,
                   brillo::KeyValueStore* metadata) {
  if (!metadata->LoadFromString(raw_metadata))
    return false;

  for (const auto& key : metadata->GetKeys()) {
    if (!IsValidKey(key))
      return false;
  }

  return true;
}

bool IsCompleteMetadata(const brillo::KeyValueStore& metadata) {
  // *.meta files always end with done=1 so we can tell if they are complete.
  std::string value;
  if (!metadata.GetString("done", &value))
    return false;
  return value == "1";
}

bool IsTimestampNewEnough(const base::FilePath& timestamp_file) {
  const base::Time threshold =
      base::Time::Now() - base::TimeDelta::FromHours(24);

  base::File::Info info;
  if (!base::GetFileInfo(timestamp_file, &info)) {
    PLOG(ERROR) << "Failed to get file info: " << timestamp_file.value();
    return false;
  }

  return threshold < info.last_modified;
}

bool IsBelowRate(const base::FilePath& timestamps_dir,
                 int max_crash_rate,
                 int* current_rate) {
  if (!base::CreateDirectory(timestamps_dir)) {
    PLOG(ERROR) << "Failed to create a timestamps directory: "
                << timestamps_dir.value();
    return false;
  }

  // Count the number of timestamp files, that were written in the past 24
  // hours. Remove files that are older.
  *current_rate = 0;
  base::FileEnumerator iter(timestamps_dir, false /* recursive */,
                            base::FileEnumerator::FILES, "*");
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    if (IsTimestampNewEnough(file)) {
      ++(*current_rate);
    } else {
      if (!base::DeleteFile(file, false /* recursive */))
        PLOG(WARNING) << "Failed to remove " << file.value();
    }
  }
  LOG(INFO) << "Current send rate: " << *current_rate << "sends/24hrs";

  if (*current_rate < max_crash_rate) {
    // It's OK to send a new crash report now. Create a new timestamp to record
    // that a new attempt is made to send a crash report.
    base::FilePath temp_file;
    if (!base::CreateTemporaryFileInDir(timestamps_dir, &temp_file)) {
      PLOG(ERROR) << "Failed to create a file in " << timestamps_dir.value();
      return false;
    }
    return true;
  }

  return false;
}

bool GetSleepTime(const base::FilePath& meta_file,
                  const base::TimeDelta& max_spread_time,
                  base::TimeDelta* sleep_time) {
  base::File::Info info;
  if (!base::GetFileInfo(meta_file, &info)) {
    PLOG(ERROR) << "Failed to get file info: " << meta_file.value();
    return false;
  }

  // The meta file should be written *after* all to-be-uploaded files that it
  // references.  Nevertheless, as a safeguard, a hold-off time of
  // kMaxHoldOffTimeInSeconds after writing the meta file is ensured.  Also,
  // sending of crash reports is spread out randomly by up to |max_spread_time|.
  // Thus, for the sleep call the greater of the two delays is used.
  const base::TimeDelta max_holdoff_time =
      base::TimeDelta::FromSeconds(kMaxHoldOffTimeInSeconds);
  // Use max() to ensure that holdoff_time is not negative.
  const base::TimeDelta holdoff_time =
      std::max(info.last_modified + max_holdoff_time - base::Time::Now(),
               base::TimeDelta());

  const int seconds = (max_spread_time.InSeconds() <= 0
                           ? 0
                           : base::RandInt(0, max_spread_time.InSeconds()));
  const base::TimeDelta spread_time = base::TimeDelta::FromSeconds(seconds);

  *sleep_time = std::max(spread_time, holdoff_time);

  return true;
}

Sender::Sender(std::unique_ptr<MetricsLibraryInterface> metrics_lib,
               const Sender::Options& options)
    : metrics_lib_(std::move(metrics_lib)),
      shell_script_(options.shell_script),
      proxy_(options.proxy),
      max_crash_rate_(options.max_crash_rate),
      max_spread_time_(options.max_spread_time),
      sleep_function_(options.sleep_function) {}

bool Sender::Init() {
  if (!scoped_temp_dir_.CreateUniqueTempDir()) {
    PLOG(ERROR) << "Failed to create a temporary directory";
    return false;
  }

  if (sleep_function_.is_null())
    sleep_function_ = base::Bind(&base::PlatformThread::Sleep);

  return true;
}

bool Sender::SendCrashes(const base::FilePath& crash_dir) {
  if (!base::DirectoryExists(crash_dir)) {
    // Directory not existing is not an error.
    return true;
  }

  RemoveOrphanedCrashFiles(crash_dir);

  std::vector<base::FilePath> to_send;
  RemoveAndPickCrashFiles(crash_dir, metrics_lib_.get(), &to_send);

  bool success = true;
  for (const auto& meta_file : to_send) {
    // This should be checked inside of the loop, since the device can enter
    // guest mode while sending crash reports with an interval up to
    // max_spread_time_ between sends.
    if (metrics_lib_->IsGuestMode()) {
      LOG(INFO) << "Guest mode has been entered. Delaying crash sending";
      return success;
    }

    int rate = 0;
    const base::FilePath timestamps_dir =
        paths::Get(paths::kTimestampsDirectory);
    if (!IsBelowRate(timestamps_dir, max_crash_rate_, &rate)) {
      LOG(INFO) << "Cannot send more crashes. Sending " << meta_file.value()
                << " would exceed the max rate: " << max_crash_rate_;
      return success;
    }

    base::TimeDelta sleep_time;
    if (!GetSleepTime(meta_file, max_spread_time_, &sleep_time)) {
      LOG(WARNING) << "Failed to compute sleep time for " << meta_file.value();
      continue;
    }

    LOG(INFO) << "Scheduled to send in " << sleep_time.InSeconds() << "s";
    if (!IsMock())
      sleep_function_.Run(sleep_time);

    if (!RequestToSendCrash(meta_file))
      success = false;
  }
  return success;
}

bool Sender::SendUserCrashes() {
  scoped_refptr<dbus::Bus> bus;
  bool fully_successful = true;

  // Set up the session manager proxy if it's not given from the options.
  if (!proxy_) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
    CHECK(bus->Connect());
    proxy_.reset(new org::chromium::SessionManagerInterfaceProxy(bus));
  }

  std::vector<base::FilePath> directories;
  if (util::GetUserCrashDirectories(proxy_.get(), &directories)) {
    for (auto directory : directories) {
      if (!SendCrashes(directory)) {
        LOG(ERROR) << "Skipped " << directory.value();
        fully_successful = false;
      }
    }
  }

  if (bus)
    bus->ShutdownAndBlock();

  return fully_successful;
}

bool Sender::RequestToSendCrash(const base::FilePath& meta_file) {
  const int child_pid = fork();
  if (child_pid == 0) {
    char* shell_script_path = const_cast<char*>(shell_script_.value().c_str());
    char* temp_dir_path =
        const_cast<char*>(scoped_temp_dir_.GetPath().value().c_str());
    char* meta_file_path = const_cast<char*>(meta_file.value().c_str());
    char* shell_argv[] = {shell_script_path, temp_dir_path, meta_file_path,
                          nullptr};
    execve(shell_script_path, shell_argv, environ);
    // execve() failed.
    exit(EXIT_FAILURE);
  } else {
    int status = 0;
    if (waitpid(child_pid, &status, 0) < 0) {
      PLOG(ERROR) << "Failed to wait for the child process: " << child_pid;
      return false;
    }
    if (!WIFEXITED(status)) {
      LOG(ERROR) << "Terminated abnormally: " << status;
      return false;
    }
    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) {
      LOG(ERROR) << "Terminated with non-zero exit code: " << exit_code;
      return false;
    }
  }

  return true;
}

}  // namespace util
