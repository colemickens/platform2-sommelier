// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UTIL_H_
#define POWER_MANAGER_UTIL_H_

namespace power_manager {

namespace util {

bool LoggedIn();
void Launch(const char* cmd);

}  // namespace util

}  // namespace power_manager

#endif  // POWER_MANAGER_UTIL_H_
