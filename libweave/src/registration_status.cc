// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/registration_status.h"

#include <base/logging.h>

namespace weave {

std::string StatusToString(RegistrationStatus status) {
  switch (status) {
    case RegistrationStatus::kUnconfigured:
      return "unconfigured";
    case RegistrationStatus::kConnecting:
      return "connecting";
    case RegistrationStatus::kConnected:
      return "connected";
    case RegistrationStatus::kInvalidCredentials:
      return "invalid_credentials";
  }
  CHECK(0) << "Unknown status";
  return "unknown";
}

}  // namespace weave
