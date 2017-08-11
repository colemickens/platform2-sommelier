// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IMAGELOADER_VERITY_MOUNTER_H_
#define IMAGELOADER_VERITY_MOUNTER_H_

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/gtest_prod_util.h>
#include <base/macros.h>

namespace imageloader {

class VerityMounter {
 public:
  VerityMounter() {}
  virtual ~VerityMounter() {}

  virtual bool Mount(const base::ScopedFD& image_fd,
                     const base::FilePath& mount_point,
                     const std::string& table);

  // Take the raw table, clean up any newlines, insert the device_path, and add
  // the correct error_condition.
  static bool SetupTable(std::string* table, const std::string& device_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(VerityMounter);
};

}  // namespace imageloader

#endif  // IMAGELOADER_VERITY_MOUNTER_H_
