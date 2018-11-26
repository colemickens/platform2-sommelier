// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is an abstraction for a Chrome OS Downloadable Content (DLC).
//
// A DLC module is a dynamically installed Chrome OS package that is verified
// via verity on device. DLC provides a way to install packages on demand
// instead of bundling all (used/unused) packages into root file system.
//
// This class verfies and mounts a DLC module image. A DLC module can be
// installed via API provided by dlc_service.

#ifndef IMAGELOADER_DLC_H_
#define IMAGELOADER_DLC_H_

#include <string>

#include <base/files/file_path.h>
#include <base/gtest_prod_util.h>

#include "imageloader/helper_process_proxy.h"

namespace imageloader {

// Enum on the two images (A/B) for one DLC module.
// We keep two copies (A/B) for each DLC module in order to sync with platform
// AutoUpdate (A/B update).
enum class AOrB { kDlcA, kDlcB, kUnknown };

class Dlc {
 public:
  explicit Dlc(const std::string& id);
  // Mount the image at |mount_point|.
  bool Mount(HelperProcessProxy* proxy,
             const std::string& a_or_b,
             const base::FilePath& mount_point);

 private:
  FRIEND_TEST_ALL_PREFIXES(DlcTest, MountDlc);
  // Mount the image from |image_path| to |mount_point| using imageloader.json
  // at |manifest_path| and table at |table_path|
  bool Mount(HelperProcessProxy* proxy,
             const base::FilePath& image_path,
             const base::FilePath& manifest_path,
             const base::FilePath& table_path,
             AOrB a_or_b,
             const base::FilePath& mount_point);
  // id of the DLC module image.
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(Dlc);
};

}  // namespace imageloader

#endif  // IMAGELOADER_DLC_H_
