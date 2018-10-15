// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_SETUP_ARC_READ_AHEAD_H_
#define ARC_SETUP_ARC_READ_AHEAD_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "arc/setup/android_sdk_version.h"

namespace base {

class FilePath;
class TimeDelta;

}  // namespace base

namespace arc {

static const int64_t kDefaultReadAheadSize = 128 * 1024;

// Tries to do what arc-uradahead.conf does with ARC++ pack file, and populates
// the kernel's page cache with files in |scan_root|. To better emulate the
// Upstart job, this function selects important files with some (not so clean)
// heuristics.
// The function returns a pair of (# of files read, # of bytes read).
std::pair<size_t, size_t> EmulateArcUreadahead(const base::FilePath& scan_root,
                                               const base::TimeDelta& timeout,
                                               AndroidSdkVersion sdk_version);

}  // namespace arc

#endif  // ARC_SETUP_ARC_READ_AHEAD_H_
