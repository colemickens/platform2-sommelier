// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DBUS_UTIL_H_
#define LOGIN_MANAGER_DBUS_UTIL_H_

#include <string>

#include <brillo/errors/error.h>

namespace login_manager {

// Creates an D-Bus error instance.
brillo::ErrorPtr CreateError(const std::string& code,
                             const std::string& message);

}  // namespace login_manager

#endif  // LOGIN_MANAGER_DBUS_UTIL_H_
