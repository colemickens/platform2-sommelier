// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/error.h"

#include <string>

#include <base/logging.h>
#include <dbus-c++/error.h>

#include "shill/dbus_adaptor.h"

namespace shill {

// static
const char * const Error::kErrorNames[Error::kNumErrors] = {
  SHILL_INTERFACE ".Error.AlreadyExists",
  SHILL_INTERFACE ".Error.InProgress",
  SHILL_INTERFACE ".Error.InternalError",
  SHILL_INTERFACE ".Error.InvalidArguments",
  SHILL_INTERFACE ".Error.InvalidNetworkName",
  SHILL_INTERFACE ".Error.InvalidPassphrase",
  SHILL_INTERFACE ".Error.InvalidProperty",
  SHILL_INTERFACE ".Error.NotFound",
  SHILL_INTERFACE ".Error.NotSupported",
  SHILL_INTERFACE ".Error.PermissionDenied",
  SHILL_INTERFACE ".Error.PinError"
};

Error::Error(Type type, const std::string& message)
    : type_(type),
      message_(message) {
  CHECK(type_ < Error::kNumErrors) << "Error type out of range: " << type;
}

Error::~Error() {}

void Error::Populate(Type type, const std::string& message) {
  CHECK(type_ < Error::kNumErrors) << "Error type out of range: " << type;
  type_ = type;
  message_ = message;
}

void Error::ToDBusError(::DBus::Error *error) {
  if (type_ < Error::kNumErrors)
    error->set(kErrorNames[type_], message_.c_str());
}

}  // namespace shill
