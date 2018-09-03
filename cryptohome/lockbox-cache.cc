// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cryptohome/lockbox-cache.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/files/file_path.h>
#include <base/logging.h>

#include "cryptohome/lockbox.h"
#include "cryptohome/lockbox-cache-tpm.h"

using base::FilePath;

namespace {
// Permissions of cache file (modulo umask).
const mode_t kCacheFilePermissions = 0644;
}

namespace cryptohome {

bool CacheLockbox(cryptohome::Platform* platform,
                  const base::FilePath& nvram_path,
                  const base::FilePath& lockbox_path,
                  const base::FilePath& cache_path) {
  static const uint32_t kBogusNvramIndex = 0;
  cryptohome::LockboxCacheTpm cache_tpm(kBogusNvramIndex, nvram_path);
  cache_tpm.Init(platform, false);
  cryptohome::LockboxCache cache;
  return cache.Initialize(platform, &cache_tpm) &&
         cache.LoadAndVerify(kBogusNvramIndex, lockbox_path) &&
         cache.Write(cache_path);
}

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
                                 const FilePath& lockbox_path) {
  if (loaded_) {
    LOG(INFO) << "Load() called in succession without a Reset()";
    return false;
  }
  if (!platform_->ReadFile(lockbox_path, &contents_)) {
    LOG(ERROR) << "Failed to read lockbox contents from "
               << lockbox_path.value();
    return false;
  }
  Lockbox lockbox(tpm_, index);
  LockboxError error;
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

bool LockboxCache::Write(const FilePath& cache_path) const {
  // Write atomically (not durably) because cache file resides on tmpfs.
  if (!platform_->WriteFileAtomic(cache_path, contents_,
                                  kCacheFilePermissions)) {
    LOG(ERROR) << "Failed to write cache file";
    return false;
  }
  return true;
}

}  // namespace cryptohome
