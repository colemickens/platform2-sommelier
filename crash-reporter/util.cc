// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/util.h"

#include <stdlib.h>

#include <map>

#include <base/files/file_util.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/cryptohome.h>
#include <brillo/key_value_store.h>
#include <vboot/crossystem.h>

#include "crash-reporter/paths.h"

namespace util {

namespace {

constexpr size_t kBufferSize = 4096;

// Path to hardware class description.
constexpr char kHwClassPath[] = "/sys/devices/platform/chromeos_acpi/HWID";

constexpr char kDevSwBoot[] = "devsw_boot";
constexpr char kDevMode[] = "dev";

}  // namespace

bool IsCrashTestInProgress() {
  return base::PathExists(paths::GetAt(paths::kSystemRunStateDirectory,
                                       paths::kCrashTestInProgress));
}

bool IsDeviceCoredumpUploadAllowed() {
  return base::PathExists(paths::GetAt(paths::kCrashReporterStateDirectory,
                                       paths::kDeviceCoredumpUploadAllowed));
}

bool IsDeveloperImage() {
  // If we're testing crash reporter itself, we don't want to special-case
  // for developer images.
  if (IsCrashTestInProgress())
    return false;
  return base::PathExists(paths::Get(paths::kLeaveCoreFile));
}

bool IsTestImage() {
  // If we're testing crash reporter itself, we don't want to special-case
  // for test images.
  if (IsCrashTestInProgress())
    return false;

  std::string channel;
  if (!GetCachedKeyValueDefault(base::FilePath(paths::kLsbRelease),
                                "CHROMEOS_RELEASE_TRACK", &channel)) {
    return false;
  }
  return base::StartsWith(channel, "test", base::CompareCase::SENSITIVE);
}

bool IsOfficialImage() {
  std::string description;
  if (!GetCachedKeyValueDefault(base::FilePath(paths::kLsbRelease),
                                "CHROMEOS_RELEASE_DESCRIPTION", &description)) {
    return false;
  }

  return description.find("Official") != std::string::npos;
}

std::string GetHardwareClass() {
  std::string hw_class;
  if (base::ReadFileToString(paths::Get(kHwClassPath), &hw_class))
    return hw_class;
  char hw_class_arr[VB_MAX_STRING_PROPERTY];
  if (!VbGetSystemPropertyString("hwid", hw_class_arr, sizeof(hw_class_arr)))
    return "undefined";
  return hw_class_arr;
}

std::string GetBootModeString() {
  // If we're testing crash reporter itself, we don't want to special-case
  // for developer mode.
  if (IsCrashTestInProgress())
    return "";

  int vb_value = VbGetSystemPropertyInt(kDevSwBoot);
  if (vb_value < 0) {
    LOG(ERROR) << "Error trying to determine boot mode";
    return "missing-crossystem";
  }
  if (vb_value == 1)
    return kDevMode;

  return "";
}

bool GetCachedKeyValue(const base::FilePath& base_name,
                       const std::string& key,
                       const std::vector<base::FilePath>& directories,
                       std::string* value) {
  std::vector<std::string> error_reasons;
  for (const auto& directory : directories) {
    const base::FilePath file_name = directory.Append(base_name);
    if (!base::PathExists(file_name)) {
      error_reasons.push_back(file_name.value() + " not found");
      continue;
    }
    brillo::KeyValueStore store;
    if (!store.Load(file_name)) {
      LOG(WARNING) << "Problem parsing " << file_name.value();
      // Even though there was some failure, take as much as we could read.
    }
    if (!store.GetString(key, value)) {
      error_reasons.push_back("Key not found in " + file_name.value());
      continue;
    }
    return true;
  }
  LOG(WARNING) << "Unable to find " << key << ": "
               << base::JoinString(error_reasons, ", ");
  return false;
}

bool GetCachedKeyValueDefault(const base::FilePath& base_name,
                              const std::string& key,
                              std::string* value) {
  const std::vector<base::FilePath> kDirectories = {
      paths::Get(paths::kCrashReporterStateDirectory),
      paths::Get(paths::kEtcDirectory),
  };
  return GetCachedKeyValue(base_name, key, kDirectories, value);
}

bool GetUserCrashDirectories(
    org::chromium::SessionManagerInterfaceProxyInterface* session_manager_proxy,
    std::vector<base::FilePath>* directories) {
  directories->clear();

  brillo::ErrorPtr error;
  std::map<std::string, std::string> sessions;
  session_manager_proxy->RetrieveActiveSessions(&sessions, &error);

  if (error) {
    LOG(ERROR) << "Error calling D-Bus proxy call to interface "
               << "'" << session_manager_proxy->GetObjectPath().value()
               << "': " << error->GetMessage();
    return false;
  }

  for (auto iter : sessions) {
    directories->push_back(
        paths::Get(brillo::cryptohome::home::GetHashedUserPath(iter.second)
                       .Append("crash")
                       .value()));
  }

  return true;
}

int RunAndCaptureOutput(brillo::ProcessImpl* process,
                        int fd,
                        std::string* output) {
  process->RedirectUsingPipe(fd, false);
  if (process->Start()) {
    const int out = process->GetPipe(fd);
    char buffer[kBufferSize];
    output->clear();

    while (true) {
      const ssize_t count = HANDLE_EINTR(read(out, buffer, kBufferSize));
      if (count < 0) {
        process->Wait();
        break;
      }

      if (count == 0)
        return process->Wait();

      output->append(buffer, count);
    }
  }

  return -1;
}

void LogMultilineError(const std::string& error) {
  std::vector<base::StringPiece> lines = base::SplitStringPiece(
      error, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (auto line : lines)
    LOG(ERROR) << line;
}

}  // namespace util
