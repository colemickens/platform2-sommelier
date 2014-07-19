// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cryptohome/lockbox-cache.h"

#include <stdio.h>
#include <unistd.h>

#include <base/logging.h>

#include "cryptohome/lockbox.h"

namespace cryptohome {

bool LockboxCache::Initialize(Platform* platform, Tpm* tpm) {
  platform_ = platform;
  tpm_ = tpm;
  Reset();
  return true;
}

void LockboxCache::Reset() {
  loaded_ = false;
  contents_.clear();
}

bool LockboxCache::LoadAndVerify(uint32_t index,
                                 const std::string& lockbox_path) {
  if (loaded_) {
    LOG(INFO) << "Load() called in succession without a Reset()";
    return false;
  }
  if (!platform_->ReadFile(lockbox_path, &contents_)) {
    LOG(ERROR) << "Failed to read lockbox contents from " << lockbox_path;
    return false;
  }
  Lockbox lockbox(tpm_, index);
  Lockbox::ErrorId error;
  if (!lockbox.Load(&error)) {
    LOG(INFO) << "Lockbox failed to load NVRAM data: " << error;
    return false;
  }
  if (!lockbox.Verify(contents_, &error)) {
    LOG(ERROR) << "Lockbox did not verify!";
    return false;
  }
  loaded_ = true;
  return true;
}

bool LockboxCache::Write(const std::string& cache_path) const {
  // Note, this writes with the caller's umask.
  if (!platform_->WriteFile(cache_path, contents_)) {
    LOG(ERROR) << "Failed to write cache file";
    return false;
  }
  return true;
}

}  // namespace cryptohome
