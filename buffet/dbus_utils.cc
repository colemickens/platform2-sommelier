// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

#include "buffet/dbus_utils.h"

namespace buffet {

namespace dbus_utils {

scoped_ptr<dbus::Response> GetBadArgsError(dbus::MethodCall* method_call,
                                           const std::string& message) {
  LOG(ERROR) << "Error while handling DBus call: " << message;
  scoped_ptr<dbus::ErrorResponse> resp(dbus::ErrorResponse::FromMethodCall(
      method_call, "org.freedesktop.DBus.Error.InvalidArgs", message));
  return scoped_ptr<dbus::Response>(resp.release());
}

}  // namespace dbus_utils

}  // namespace buffet
