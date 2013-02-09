// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_UTIL_H_
#define POWER_MANAGER_COMMON_UTIL_H_

#include <string>

#include "power_manager/common/power_constants.h"

class FilePath;

typedef unsigned int guint;

namespace power_manager {
namespace util {

bool OOBECompleted();

// Issue command asynchronously.
void Launch(const char* command);

// Issue command synchronously.
void Run(const char* command);

// Runs powerd_setuid_helper.  |action| is passed via --action.  If
// |additional_args| is non-empty, it will be appended to the command.
// If |wait_for_completion| is true, this function will block until the helper
// finishes; otherwise it will return immediately.
void RunSetuidHelper(const std::string& action,
                     const std::string& additional_args,
                     bool wait_for_completion);

// Status file creation and removal.
void CreateStatusFile(const FilePath& file);
void RemoveStatusFile(const FilePath& file);

// Read an unsigned int from a file.  Return true on success
// Due to crbug.com/128596 this function does not handle negative values
// in the file well.  They are read in as signed values and then cast
// to unsigned ints.  So -10 => 4294967286
bool GetUintFromFile(const char* filename, unsigned int* value);

// Returns a string describing |type|.
const char* InputTypeToString(InputType type);

// Deletes a GLib timeout ID via g_source_remove() and resets the variable
// containing it to 0.  Does nothing if |timeout_id| points at a non-zero ID.
void RemoveTimeout(guint* timeout_id);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_UTIL_H_
