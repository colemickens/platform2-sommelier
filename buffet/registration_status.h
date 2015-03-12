// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_REGISTRATION_STATUS_H_
#define BUFFET_REGISTRATION_STATUS_H_

#include <string>

namespace buffet {

// See the DBus interface XML file for complete descriptions of these states.
enum class RegistrationStatus {
  kUnconfigured,        // We have no credentials.
  kConnecting,          // We have credentials but not yet connected.
  kConnected,           // We're registered and connected to the cloud.
  kInvalidCredentials,  // Our registration has been revoked.
};

std::string StatusToString(RegistrationStatus status);

}  // namespace buffet

#endif  // BUFFET_REGISTRATION_STATUS_H_
