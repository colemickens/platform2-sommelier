// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_UTIL_H_
#define CRASH_REPORTER_UTIL_H_

namespace util {

// Returns true if integration tests are currently running.
bool IsCrashTestInProgress();

// Returns true if running on a developer image.
bool IsDeveloperImage();

}  // namespace util

#endif  // CRASH_REPORTER_UTIL_H_
