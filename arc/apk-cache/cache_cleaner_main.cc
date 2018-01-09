// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/apk-cache/cache_cleaner.h"

#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>

namespace {

constexpr char kApkCacheDir[] = "/mnt/stateful_partition/unencrypted/cache/apk";
constexpr char kHelpText[] =
    "Performs cleaning of the APK cache directory: "
    "/mnt/stateful_partition/unencrypted/cache/apk/\n"
    "It removes:\n"
    " - all the files in the cache root;\n"
    " - all the package directories that:\n"
    "   1. have not been used within last 30 days;\n"
    "   2. contain unexpected files. Any file except APK, main and patch OBB\n"
    "      and JSON with package attributes is considered unexpected;\n"
    "   3. contain directories;\n"
    "   4. contain no or more than one APK file, no attributes JSON file,\n"
    "      more then one main OBB file, more then one patch OBB file.\n"
    "Returns 0 all the intended files and directories were successfully\n"
    "deleted.";

}  // namespace

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv, kHelpText);

  if (!apk_cache::Clean(base::FilePath(kApkCacheDir))) {
    LOG(ERROR) << "APK Cache cleaner experienced problem while cleaning.";
    return 1;
  }

  LOG(INFO) << "APK Cache cleaner succeeded.";
  return 0;
}
