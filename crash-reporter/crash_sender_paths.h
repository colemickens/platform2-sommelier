// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CRASH_SENDER_PATHS_H_
#define CRASH_REPORTER_CRASH_SENDER_PATHS_H_

#include <base/files/file_path.h>
#include <base/strings/string_piece.h>

namespace paths {

// The path constants here should be used with Get() function below.

// File whose existence mocks crash sending.  If empty we pretend the
// crash sending was successful, otherwise unsuccessful.
constexpr char kMockCrashSending[] = "/run/crash_reporter/mock-crash-sending";

// Crash sender lock in case the sender is already running.
constexpr char kLockFile[] = "/run/lock/crash_sender";

// File whose existence implies we're running and not to start again.
constexpr char kRunFile[] = "/run/crash_sender.pid";

// Gets a FilePath from the given path. A prefix will be added if the prefix is
// set with SetPrefixForTesting().
base::FilePath Get(base::StringPiece file_path);

// Sets a prefix that'll be added when Get() is called, for unit testing.
// For example, if "/tmp" is set as the prefix, Get("/run/foo") will return
// "/tmp/run/foo". Passing "" will reset the prefix.
void SetPrefixForTesting(const base::FilePath& prefix);

}  // namespace paths

#endif  // CRASH_REPORTER_CRASH_SENDER_PATHS_H_
