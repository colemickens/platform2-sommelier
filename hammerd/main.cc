// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// hammerd - A daemon to update the firmware of Hammer

#include <stdlib.h>

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>

#include "hammerd/hammer_updater.h"
#include "hammerd/process_lock.h"

namespace {
// The lock file used to prevent multiple hammerd be invoked at the same time.
const char kLockFile[] = "/run/lock/hammerd.lock";
// The path of default EC firmware.
const char kDefaultImagePath[] = "/lib/firmware/hammer.fw";
}  // namespace

int main(int argc, const char* argv[]) {
  DEFINE_string(image, kDefaultImagePath, "The path of the image file.");
  brillo::FlagHelper::Init(argc, argv, "Hammer EC firmware updater daemon.");

  base::FilePath file_path(FILE_PATH_LITERAL(kLockFile));
  hammerd::ProcessLock lock(file_path);
  if (!lock.Acquire()) {
    LOG(INFO) << "Other hammerd process has been executed, exit.";
    return EXIT_SUCCESS;
  }
  std::string image;
  if (!base::ReadFileToString(base::FilePath(FLAGS_image), &image)) {
    LOG(ERROR) << "Image file is not found: " << FLAGS_image;
    lock.Release();
    return EXIT_FAILURE;
  }

  hammerd::HammerUpdater updater(image);
  bool ret = updater.Run();
  lock.Release();
  return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
