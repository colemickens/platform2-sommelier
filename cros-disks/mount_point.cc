// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mount_point.h"

#include <utility>

#include "cros-disks/error_logger.h"
#include "cros-disks/mounter.h"
#include "cros-disks/quote.h"

namespace cros_disks {

MountPoint::MountPoint(const base::FilePath& path,
                       std::unique_ptr<Unmounter> unmounter)
    : path_(path), unmounter_(std::move(unmounter)) {}

MountPoint::~MountPoint() {
  Unmount();
}

void MountPoint::Release() {
  unmounter_.reset();
}

MountErrorType MountPoint::Unmount() {
  if (!unmounter_) {
    return MOUNT_ERROR_PATH_NOT_MOUNTED;
  }
  MountErrorType error = unmounter_->Unmount(*this);
  if (error != MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Cannot unmount " << quote(path_) << ": " << error;
    // Leave |unmounter_| as we might want to retry later.
  } else {
    unmounter_.reset();
  }
  return error;
}

}  // namespace cros_disks
