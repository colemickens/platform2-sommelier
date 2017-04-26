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

int main(int argc, const char* argv[]) {
  DEFINE_string(image, "", "The path of the image file.");
  brillo::FlagHelper::Init(argc, argv, "Hammer EC firmware updater daemon.");

  if (!FLAGS_image.length()) {
    LOG(FATAL) << "No image file is assigned.";
    return EXIT_FAILURE;
  }
  std::string image;
  if (!base::ReadFileToString(base::FilePath(FLAGS_image), &image)) {
    LOG(FATAL) << "Image file is not found: " << FLAGS_image;
    return EXIT_FAILURE;
  }

  hammerd::HammerUpdater updater(image);
  return updater.Run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
