// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_PRIVET_TYPES_H_
#define PRIVETD_PRIVET_TYPES_H_

#include <string>

namespace privetd {

enum class Error {
  kNone,
  kInvalidFormat,
  kMissingAuthorization,
  kInvalidAuthorization,
  kInvalidAuthorizationScope,
  kCommitmentMismatch,
  kUnknownSession,
  kInvalidAuthCode,
  kInvalidAuthMode,
  kInvalidRequestedScope,
  kAccessDenied,
  kInvalidParams,
  kSetupUnavailable,
  kDeviceBusy,
  kInvalidTicket,
  kServerError,
  kDeviceConfigError,
  kInvalidSsid,
  kInvalidPassphrase,
};

struct ConnectionState {
  enum Status {
    kDisabled,
    kUnconfigured,
    kConnecting,
    kOnline,
    kOffline,
    kError,
  };

  explicit ConnectionState(Status status) : status(status) {}

  explicit ConnectionState(Error error) : status(kError), error(error) {}

  ConnectionState(Error error, const std::string& error_message)
      : status(kError), error(error), error_message(error_message) {}

  Status status;
  Error error = Error::kNone;
  std::string error_message;
};

struct SetupState {
  enum Status {
    kNone,
    kInProgress,
    kSuccess,
    kError,
  };

  explicit SetupState(Status status) : status(status) {}

  explicit SetupState(Error error) : status(kError), error(error) {}

  SetupState(Error error, const std::string& error_message)
      : status(kError), error(error), error_message(error_message) {}

  Status status;
  Error error = Error::kNone;
  std::string error_message;
};

}  // namespace privetd

#endif  // PRIVETD_PRIVET_TYPES_H_
