// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_LOCKBOX_CACHE_H_
#define CRYPTOHOME_LOCKBOX_CACHE_H_

#include <stdio.h>
#include <unistd.h>

#include <string>

#include <base/files/file_path.h>
#include <base/logging.h>

#include "cryptohome/platform.h"
#include "cryptohome/tpm.h"

namespace cryptohome {

// Verify the lockbox contents at |lockbox_path| against the NVRAM space
// contents at |nvram_path| and write the lockbox contents to |cache_path| upon
// successful verification. Return value indicates verification status.
bool CacheLockbox(cryptohome::Platform* platform,
                  const base::FilePath& nvram_path,
                  const base::FilePath& lockbox_path,
                  const base::FilePath& cache_path);

// Implements a simple writer wrapper for arbitrary Lockboxes
class LockboxCache {
 public:
  LockboxCache() : loaded_(false), tpm_(NULL), platform_(NULL) {}
  virtual ~LockboxCache() {}
  // Prepares the cache for use.
  // Does not take ownership of any pointers.
  virtual bool Initialize(Platform* platform, Tpm* tpm);
  // Resets the cache for another Load->Write cycle.
  virtual void Reset();
  // Loads the supplied |lockbox_path| and validates it against the TPM.
  // If the data loads and verifies, returns true;
  virtual bool LoadAndVerify(uint32_t index,
                             const base::FilePath& lockbox_path);
  // Iff LoadAndVerify() returned true, will write out the loaded lockbox
  // contents to |cache_path|.
  virtual bool Write(const base::FilePath& cache_path) const;
 private:
  bool loaded_;
  Tpm* tpm_;
  Platform* platform_;
  brillo::Blob contents_;
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_LOCKBOX_CACHE_H_
