// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdio.h>
#include <unistd.h>

#include <base/logging.h>

#include "platform.h"
#include "tpm.h"

namespace cryptohome {

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
  virtual bool LoadAndVerify(uint32_t index, const std::string& lockbox_path);
  // Iff LoadAndVerify() returned true, will write out the loaded lockbox
  // contents to |cache_path|.
  virtual bool Write(const std::string& cache_path) const;
 private:
  bool loaded_;
  Tpm* tpm_;
  Platform* platform_;
  chromeos::Blob contents_;
};
}  // namespace cryptohome
