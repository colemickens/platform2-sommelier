// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_PATHS_H_
#define CRASH_REPORTER_PATHS_H_

#include <base/files/file_path.h>
#include <base/strings/string_piece.h>

namespace paths {

// Directory where we keep various state flags.
constexpr char kSystemRunStateDirectory[] = "/run/crash_reporter";

// Directory where crash_reporter stores files (ex. saved version info).
constexpr char kCrashReporterStateDirectory[] = "/var/lib/crash_reporter";

// Directory where system crashes are saved.
constexpr char kSystemCrashDirectory[] = "/var/spool/crash";

// Directory where system configuration files are located.
constexpr char kEtcDirectory[] = "/etc";

// File whose existence indicates this is a developer image.
constexpr char kLeaveCoreFile[] = "/root/.leave_core";

// Base name of file whose existence indicates a crash test is currently
// running.
constexpr char kCrashTestInProgress[] = "crash-test-in-progress";

// Base name of file that contains Chrome OS version info.
constexpr char kLsbRelease[] = "lsb-release";

// Gets a FilePath from the given path. A prefix will be added if the prefix is
// set with SetPrefixForTesting().
base::FilePath Get(base::StringPiece file_path);

// Gets a FilePath from the given directory and the base name. A prefix will be
// added if the prefix is set with SetPrefixForTesting().
base::FilePath GetAt(base::StringPiece directory, base::StringPiece base_name);

// Sets a prefix that'll be added when Get() is called, for unit testing.
// For example, if "/tmp" is set as the prefix, Get("/run/foo") will return
// "/tmp/run/foo". Passing "" will reset the prefix.
void SetPrefixForTesting(const base::FilePath& prefix);

}  // namespace paths

#endif  // CRASH_REPORTER_PATHS_H_
