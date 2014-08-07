// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_

#include <string>

#include <base/memory/scoped_ptr.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

namespace chromeos {

namespace dbus_utils {

scoped_ptr<dbus::Response> GetBadArgsError(dbus::MethodCall* method_call,
                                           const std::string& message);

}  // namespace dbus_utils

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_
