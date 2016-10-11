// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_SCOPED_UMASK_H_
#define BIOD_SCOPED_UMASK_H_

#include <sys/types.h>

#include <base/macros.h>

namespace biod {

class ScopedUmask {
 public:
  explicit ScopedUmask(mode_t new_umask);
  ~ScopedUmask();

 private:
  mode_t saved_umask_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUmask);
};

}  // namespace biod

#endif  // BIOD_SCOPED_UMASK_H_
