// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOUNT_UTILS_H_
#define CRYPTOHOME_MOUNT_UTILS_H_

#include "cryptohome/platform.h"

namespace cryptohome {

// A helper class for scoping umask changes.
class ScopedUmask {
 public:
  ScopedUmask(Platform* platform, int mask)
      : platform_(platform),
        old_mask_(platform_->SetMask(mask)) {}
  ~ScopedUmask() {platform_->SetMask(old_mask_);}
 private:
  Platform* platform_;
  int old_mask_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOUNT_UTILS_H_
