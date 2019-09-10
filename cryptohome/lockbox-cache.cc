// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/lockbox-cache.h"

#include <memory>

#include <base/files/file_path.h>
#include <base/logging.h>

#include "cryptohome/cryptohome_metrics.h"
#include "cryptohome/lockbox.h"

namespace cryptohome {
namespace {
// Permissions of cache file (modulo umask).
const mode_t kCacheFilePermissions = 0644;
}

bool CacheLockbox(cryptohome::Platform* platform,
                  const base::FilePath& nvram_path,
                  const base::FilePath& lockbox_path,
                  const base::FilePath& cache_path) {
  brillo::SecureBlob nvram;
  if (!platform->ReadFileToSecureBlob(nvram_path, &nvram)) {
    LOG(INFO) << "Failed to read NVRAM contents from " << nvram_path.value();
    ReportInstallAttributesValidation(
        InstallAttributesValidationEvent::kNvramReadFailed);
    return false;
  }
  std::unique_ptr<LockboxContents> lockbox = LockboxContents::New(nvram.size());
  if (!lockbox) {
    LOG(ERROR) << "Unsupported lockbox size!";
    ReportInstallAttributesValidation(
        InstallAttributesValidationEvent::kNvramInvalidSizeRead);
    return false;
  }
  if (!lockbox->Decode(nvram)) {
    LOG(ERROR) << "Lockbox failed to decode NVRAM data";
    ReportInstallAttributesValidation(
        InstallAttributesValidationEvent::kNvramDecodeFailed);
    return false;
  }

  brillo::Blob lockbox_data;
  if (!platform->ReadFile(lockbox_path, &lockbox_data)) {
    LOG(INFO) << "Failed to read lockbox data from " << lockbox_path.value();
    ReportInstallAttributesValidation(
        InstallAttributesValidationEvent::kDataReadFailed);
    return false;
  }

  LockboxContents::VerificationResult result = lockbox->Verify(lockbox_data);

  if (result != LockboxContents::VerificationResult::kValid) {
    switch (result) {
      case LockboxContents::VerificationResult::kSizeMismatch:
        LOG(ERROR) << "Lockbox did not verify due to invalid size!";
        ReportInstallAttributesValidation(
            InstallAttributesValidationEvent::kDataVerificationSizeFailed);
        break;

      case LockboxContents::VerificationResult::kHashMismatch:
        LOG(ERROR) << "Lockbox did not verify due to invalid hash!";
        ReportInstallAttributesValidation(
            InstallAttributesValidationEvent::kDataVerificationHashFailed);
        break;

      default:
        NOTREACHED() << "Unexpected LockboxContents::VerificationResult";
        break;
    }

    return false;
  }

  // Write atomically (not durably) because cache file resides on tmpfs.
  if (!platform->WriteFileAtomic(cache_path, lockbox_data,
                                 kCacheFilePermissions)) {
    LOG(ERROR) << "Failed to write cache file";
    ReportInstallAttributesValidation(
        InstallAttributesValidationEvent::kCacheWriteFailed);
    return false;
  }

  ReportInstallAttributesValidation(
      InstallAttributesValidationEvent::kCacheWriteSucceeded);
  return true;
}

}  // namespace cryptohome
