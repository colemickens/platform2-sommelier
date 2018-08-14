// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/util.h"

#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <brillo/key_value_store.h>

#include "crash-reporter/paths.h"

namespace util {

bool IsCrashTestInProgress() {
  return base::PathExists(paths::GetAt(paths::kSystemRunStateDirectory,
                                       paths::kCrashTestInProgress));
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
      // TODO(bmgordon): Remove this fallback here and in
      // crash_sender around 2019-01-01. See also
      // CrashCollector::GetVersion().
      paths::Get(paths::kSystemCrashDirectory),
      paths::Get(paths::kEtcDirectory),
  };
  return GetCachedKeyValue(base_name, key, kDirectories, value);
}

}  // namespace util
