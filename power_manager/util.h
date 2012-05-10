// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UTIL_H_
#define POWER_MANAGER_UTIL_H_

#include <X11/Xlib.h>

class FilePath;

namespace power_manager {
namespace util {

bool OOBECompleted();

// Issue command asynchronously.
void Launch(const char* command);

// Status file creation and removal.
void CreateStatusFile(const FilePath& file);
void RemoveStatusFile(const FilePath& file);

// Get the current wakeup count from sysfs
bool GetWakeupCount(unsigned int* value);

// Get a connection to the X server. Opens a connection the first time it's
// called and caches it.
Display* GetDisplay();

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_UTIL_H_
