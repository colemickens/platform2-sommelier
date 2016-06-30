// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_stack.h"

#include <algorithm>
#include <base/files/file_path.h>
#include <base/logging.h>

using base::FilePath;

MountStack::MountStack() { }
MountStack::~MountStack() {
  if (!mounts_.empty()) {
    LOG(ERROR) << "MountStack destroyed with " << mounts_.size() << "mounts.";
    std::vector<FilePath>::iterator it;
    for (it = mounts_.begin(); it != mounts_.end(); ++it)
      LOG(ERROR) << "  " << it->value();
  }
}

void MountStack::Push(const FilePath& path) {
  mounts_.push_back(path);
}

bool MountStack::Pop(FilePath* path) {
  if (mounts_.empty())
    return false;
  *path = mounts_.back();
  mounts_.pop_back();
  return true;
}

bool MountStack::Contains(const FilePath& path) const {
  return std::find(mounts_.begin(), mounts_.end(), path) != mounts_.end();
}
