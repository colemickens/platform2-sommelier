// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/error.h"

#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>

#if defined(__ANDROID__)
#include <dbus/service_constants.h>
#else
#include <chromeos/dbus/service_constants.h>
#endif  // __ANDROID__

using std::string;

namespace apmanager {

Error::Error() : type_(kSuccess) {}

Error::~Error() {}

void Error::Populate(Type type,
                     const string& message,
                     const tracked_objects::Location& location) {
  CHECK(type < kNumErrors) << "Error type out of range: " << type;
  type_ = type;
  message_ = message;
  location_ = location;
}

void Error::Reset() {
  type_ = kSuccess;
  message_ = "";
  location_ = tracked_objects::Location();
}

bool Error::ToDBusError(brillo::ErrorPtr* error) const {
  if (IsSuccess()) {
    return false;
  }

  string error_code = kErrorInternalError;
  if (type_ == kInvalidArguments) {
    error_code = kErrorInvalidArguments;
  } else if (type_ == kInvalidConfiguration) {
    error_code = kErrorInvalidConfiguration;
  }

  brillo::Error::AddTo(error,
                       location_,
                       brillo::errors::dbus::kDomain,
                       error_code,
                       message_);
  return true;
}

// static
void Error::PopulateAndLog(Error* error,
                           Type type,
                           const string& message,
                           const tracked_objects::Location& from_here) {
  string file_name = base::FilePath(from_here.file_name()).BaseName().value();
  LOG(ERROR) << "[" << file_name << "("
             << from_here.line_number() << ")]: "<< message;
  if (error) {
    error->Populate(type, message, from_here);
  }
}

}  // namespace apmanager
