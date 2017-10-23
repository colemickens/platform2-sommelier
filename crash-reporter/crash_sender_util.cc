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
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
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

void ParseCommandLine(int argc, const char* const* argv) {
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
  brillo::FlagHelper::Init(new_argv.size() - 1, new_argv.data(),
                           "Chromium OS Crash Sender");
  // TODO(satorux): Remove this once -e option is gone.
  if (FLAGS_h)
    ShowUsageAndExit();

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
    if (!GetFileInfo(file, &info)) {
      LOG(ERROR) << "Failed to get file info: " << file.value();
      continue;
    }
    base::TimeDelta delta = base::Time::Now() - info.last_modified;

    if (!base::PathExists(meta_file) && delta.InHours() >= 24) {
      LOG(INFO) << "Removing old orphaned file: " << file.value();
      if (!base::DeleteFile(file, false /* recursive */))
        PLOG(ERROR) << "Failed to remove " << file.value();
    }
  }
}

bool ShouldRemove(const base::FilePath& meta_file, std::string* reason) {
  if (!IsMock() && !IsOfficialImage()) {
    *reason = "Not an official OS version";
    return true;
  }

  std::string raw_metadata;
  if (!base::ReadFileToString(meta_file, &raw_metadata)) {
    PLOG(WARNING) << "Igonoring: metadata file is inaccessible";
    return false;
  }

  brillo::KeyValueStore metadata;
  if (!ParseMetadata(raw_metadata, &metadata)) {
    *reason = "Corrupted metadata: " + raw_metadata;
    return true;
  }

  base::FilePath payload_path = GetBaseNameFromMetadata(metadata, "payload");
  if (payload_path.empty()) {
    *reason = "Payload is not found in the meta data: " + raw_metadata;
    return true;
  }

  // Make it an absolute path.
  payload_path = meta_file.DirName().Append(payload_path);

  if (!base::PathExists(payload_path)) {
    // TODO(satorux): logging_CrashSender.py expects "Missing payload" in the
    // error message. Revise the autotest once the rewrite to C++ is complete.
    *reason = "Missing payload: " + payload_path.value();
    return true;
  }

  const std::string kind = GetKindFromPayloadPath(payload_path);
  if (!IsKnownKind(kind)) {
    *reason = "Unknown kind: " + kind;
    return true;
  }

  return false;
}

void RemoveInvalidCrashFiles(const base::FilePath& crash_dir) {
  std::vector<base::FilePath> meta_files = GetMetaFiles(crash_dir);

  for (const auto& meta_file : meta_files) {
    LOG(INFO) << "Checking metadata: " << meta_file.value();

    std::string reason;
    if (ShouldRemove(meta_file, &reason)) {
      LOG(ERROR) << "Removing: " << reason;
      RemoveReportFiles(meta_file);
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
      PLOG(ERROR) << "Failed to remove " << file.value();
  }
}

std::vector<base::FilePath> GetMetaFiles(const base::FilePath& crash_dir) {
  base::FileEnumerator iter(crash_dir, false /* recursive */,
                            base::FileEnumerator::FILES, "*.meta");
  std::vector<std::pair<base::Time, base::FilePath>> time_meta_pairs;
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    base::File::Info info;
    if (!GetFileInfo(file, &info)) {
      LOG(ERROR) << "Failed to get file info: " << file.value();
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

Sender::Sender(const Sender::Options& options)
    : shell_script_(options.shell_script), proxy_(options.proxy) {}

bool Sender::Init() {
  if (!scoped_temp_dir_.CreateUniqueTempDir()) {
    PLOG(ERROR) << "Failed to create a temporary directory.";
    return false;
  }
  return true;
}

bool Sender::SendCrashes(const base::FilePath& crash_dir) {
  if (!base::DirectoryExists(crash_dir)) {
    // Directory not existing is not an error.
    return true;
  }

  RemoveOrphanedCrashFiles(crash_dir);
  RemoveInvalidCrashFiles(crash_dir);

  const int child_pid = fork();
  if (child_pid == 0) {
    char* shell_script_path = const_cast<char*>(shell_script_.value().c_str());
    char* temp_dir_path =
        const_cast<char*>(scoped_temp_dir_.GetPath().value().c_str());
    char* crash_dir_path = const_cast<char*>(crash_dir.value().c_str());
    char* shell_argv[] = {shell_script_path, temp_dir_path, crash_dir_path,
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

}  // namespace util
