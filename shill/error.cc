// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/error.h"

#include <base/logging.h>
#include <base/stringprintf.h>
#include <dbus-c++/error.h>

#include "shill/dbus_adaptor.h"

using std::string;

namespace shill {

// static
const Error::Info Error::kInfos[kNumErrors] = {
  { "Success", "Success (no error)" },
  { "AlreadyConnected", "Already connected" },
  { "AlreadyExists", "Already exists" },
  { "InProgress", "In progress" },
  { "InternalError", "Internal error" },
  { "InvalidArguments", "Invalid arguments" },
  { "InvalidNetworkName", "Invalid network name" },
  { "InvalidPassphrase", "Invalid passphrase" },
  { "InvalidProperty", "Invalid property" },
  { "NoCarrier", "No carrier" },
  { "NotConnected", "Not connected" },
  { "NotFound", "Not found" },
  { "NotImplemented", "Not implemented" },
  { "NotOnHomeNetwork", "Not on home network" },
  { "NotRegistered", "Not registered" },
  { "NotSupported", "Not supported" },
  { "OperationAborted", "Operation aborted" },
  { "OperationTimeout", "Operation timeout" },
  { "PassphraseRequired", "Passphrase required" },
  { "PermissionDenied", "Permission denied" },
};

// static
const char Error::kInterfaceName[] = SHILL_INTERFACE;

Error::Error() {
  Populate(kSuccess);
}

Error::Error(Type type) {
  Populate(type);
}

Error::Error(Type type, const string &message) {
  Populate(type, message);
}

Error::~Error() {}

void Error::Populate(Type type) {
  Populate(type, GetDefaultMessage(type));
}

void Error::Populate(Type type, const string &message) {
  CHECK(type < kNumErrors) << "Error type out of range: " << type;
  type_ = type;
  message_ = message;
}

bool Error::ToDBusError(::DBus::Error *error) const {
  if (IsFailure()) {
    error->set(GetName(type_).c_str(), message_.c_str());
    return true;
  } else {
    return false;
  }
}

// static
string Error::GetName(Type type) {
  CHECK(type < kNumErrors) << "Error type out of range: " << type;
  return base::StringPrintf("%s.Error.%s", kInterfaceName, kInfos[type].name);
}

// static
string Error::GetDefaultMessage(Type type) {
  CHECK(type < kNumErrors) << "Error type out of range: " << type;
  return kInfos[type].message;
}

// static
void Error::PopulateAndLog(Error *error, Type type, const string &message) {
  LOG(ERROR) << message;
  if (error) {
    error->Populate(type, message);
  }
}

}  // namespace shill
