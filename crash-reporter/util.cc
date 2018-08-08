// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/util.h"

#include <base/files/file_util.h>

#include "crash-reporter/paths.h"

namespace util {

bool IsCrashTestInProgress() {
  return base::PathExists(paths::GetAt(paths::kSystemRunStateDirectory,
                                       paths::kCrashTestInProgress));
}

bool IsDeveloperImage() {
  // If we're testing crash reporter itself, we don't want to special-case
  // for developer images.
  if (IsCrashTestInProgress())
    return false;
  return base::PathExists(paths::Get(paths::kLeaveCoreFile));
}

}  // namespace util
