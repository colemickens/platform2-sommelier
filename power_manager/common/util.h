// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_UTIL_H_
#define POWER_MANAGER_COMMON_UTIL_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/time.h"

typedef unsigned int guint;

namespace power_manager {
namespace util {

bool OOBECompleted();

// Runs |command| asynchronously.
void Launch(const char* command);

// Runs |command| synchronously.  The process's exit code is returned.
int Run(const char* command);

// Runs powerd_setuid_helper.  |action| is passed via --action.  If
// |additional_args| is non-empty, it will be appended to the command.
// If |wait_for_completion| is true, this function will block until the helper
// finishes and return the helper's exit code; otherwise it will return 0
// immediately.
int RunSetuidHelper(const std::string& action,
                    const std::string& additional_args,
                    bool wait_for_completion);

// Read an unsigned int from a file.  Return true on success
// Due to crbug.com/128596 this function does not handle negative values
// in the file well.  They are read in as signed values and then cast
// to unsigned ints.  So -10 => 4294967286
bool GetUintFromFile(const char* filename, unsigned int* value);

// Deletes a GLib timeout ID via g_source_remove() and resets the variable
// containing it to 0.  Does nothing if |timeout_id| points at a non-zero ID.
void RemoveTimeout(guint* timeout_id);

// Clamps |percent| in the range [0.0, 100.0].
double ClampPercent(double percent);

// Returns |delta| as a string of the format "4h3m45s".
std::string TimeDeltaToString(base::TimeDelta delta);

// Returns a list of paths to pass when creating a Prefs object. For a given
// preference, |read_write_path| will be checked first, then the board-specific
// subdirectory within |read_only_path|, and finally |read_only_path|.
std::vector<base::FilePath> GetPrefPaths(const std::string& read_write_path,
                                         const std::string& read_only_path);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_UTIL_H_
