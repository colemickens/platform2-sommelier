// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_TYPES_H_
#define LIBWEAVE_INCLUDE_WEAVE_TYPES_H_

#include <string>

namespace weave {

// See the DBus interface XML file for complete descriptions of these states.
enum class RegistrationStatus {
  kUnconfigured,        // We have no credentials.
  kConnecting,          // We have credentials but not yet connected.
  kConnected,           // We're registered and connected to the cloud.
  kInvalidCredentials,  // Our registration has been revoked.
};

enum class PairingType {
  kPinCode,
  kEmbeddedCode,
  kUltrasound32,
  kAudible32,
};

enum class WifiSetupState {
  kDisabled,
  kBootstrapping,
  kMonitoring,
  kConnecting,
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_TYPES_H_
