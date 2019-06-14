// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/error.h"

#include <base/files/file_path.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/logging.h"

using std::string;

namespace shill {

namespace {

struct Info {
  const char* dbus_result;  // Error type name
  const char* message;      // Default error type message
};

const Info kInfos[Error::kNumErrors] = {
    {kErrorResultSuccess, "Success (no error)"},
    {kErrorResultFailure, "Operation failed (no other information)"},
    {kErrorResultAlreadyConnected, "Already connected"},
    {kErrorResultAlreadyExists, "Already exists"},
    {kErrorResultIncorrectPin, "Incorrect PIN"},
    {kErrorResultInProgress, "In progress"},
    {kErrorResultInternalError, "Internal error"},
    {kErrorResultInvalidApn, "Invalid APN"},
    {kErrorResultInvalidArguments, "Invalid arguments"},
    {kErrorResultInvalidNetworkName, "Invalid network name"},
    {kErrorResultInvalidPassphrase, "Invalid passphrase"},
    {kErrorResultInvalidProperty, "Invalid property"},
    {kErrorResultNoCarrier, "No carrier"},
    {kErrorResultNotConnected, "Not connected"},
    {kErrorResultNotFound, "Not found"},
    {kErrorResultNotImplemented, "Not implemented"},
    {kErrorResultNotOnHomeNetwork, "Not on home network"},
    {kErrorResultNotRegistered, "Not registered"},
    {kErrorResultNotSupported, "Not supported"},
    {kErrorResultOperationAborted, "Operation aborted"},
    {kErrorResultOperationInitiated, "Operation initiated"},
    {kErrorResultOperationTimeout, "Operation timeout"},
    {kErrorResultPassphraseRequired, "Passphrase required"},
    {kErrorResultPermissionDenied, "Permission denied"},
    {kErrorResultPinBlocked, "SIM PIN is blocked"},
    {kErrorResultPinRequired, "SIM PIN is required"},
    {kErrorResultWrongState, "Wrong state"},
};

}  // namespace

Error::Error() {
  Reset();
}

Error::Error(Type type) {
  Populate(type);
}

Error::Error(Type type, const string& message) {
  Populate(type, message);
}

Error::~Error() = default;

void Error::Populate(Type type) {
  Populate(type, GetDefaultMessage(type));
}

void Error::Populate(Type type, const string& message) {
  CHECK(type < kNumErrors) << "Error type out of range: " << type;
  type_ = type;
  message_ = message;
}

void Error::Populate(Type type,
                     const string& message,
                     const base::Location& location) {
  CHECK(type < kNumErrors) << "Error type out of range: " << type;
  type_ = type;
  message_ = message;
  location_ = location;
}

void Error::Reset() {
  Populate(kSuccess);
}

void Error::CopyFrom(const Error& error) {
  Populate(error.type_, error.message_);
}

bool Error::ToChromeosError(brillo::ErrorPtr* error) const {
  if (IsFailure()) {
    brillo::Error::AddTo(error,
                         location_,
                         brillo::errors::dbus::kDomain,
                         kInfos[type_].dbus_result,
                         message_);
    return true;
  }
  return false;
}

// static
string Error::GetDBusResult(Type type) {
  CHECK(type < kNumErrors) << "Error type out of range: " << type;
  return kInfos[type].dbus_result;
}

// static
string Error::GetDefaultMessage(Type type) {
  CHECK(type < kNumErrors) << "Error type out of range: " << type;
  return kInfos[type].message;
}

// static
void Error::PopulateAndLog(const base::Location& from_here,
                           Error* error, Type type, const string& message) {
  string file_name = base::FilePath(from_here.file_name()).BaseName().value();
  LOG(ERROR) << "[" << file_name << "("
             << from_here.line_number() << ")]: "<< message;
  if (error) {
    error->Populate(type, message, from_here);
  }
}

}  // namespace shill

std::ostream& operator<<(std::ostream& stream, const shill::Error& error) {
  stream << error.GetDBusResult(error.type()) << ": " << error.message();
  return stream;
}
