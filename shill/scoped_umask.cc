// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/scoped_umask.h"

#include <sys/stat.h>

namespace shill {

ScopedUmask::ScopedUmask(mode_t new_umask) {
  saved_umask_ = umask(new_umask);
}

ScopedUmask::~ScopedUmask() {
  umask(saved_umask_);
}

}  // namespace shill
