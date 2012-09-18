// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UTIL_H_
#define POWER_MANAGER_UTIL_H_

#include "power_manager/backlight_controller.h"

class FilePath;

namespace power_manager {
namespace util {

bool OOBECompleted();

// Issue command asynchronously.
void Launch(const char* command);

// Issue command synchronously.
void Run(const char* command);

// Status file creation and removal.
void CreateStatusFile(const FilePath& file);
void RemoveStatusFile(const FilePath& file);

// Get the current wakeup count from sysfs
bool GetWakeupCount(unsigned int* value);

// Read an unsigned int from a file.  Return true on success
// Due to crbug.com/128596 this function does not handle negative values
// in the file well.  They are read in as signed values and then cast
// to unsigned ints.  So -10 => 4294967286
bool GetUintFromFile(const char* filename, unsigned int* value);

// String names for power states.
const char* PowerStateToString(PowerState state);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_UTIL_H_
