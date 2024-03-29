// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ERROR_H_
#define SHILL_ERROR_H_

#include <memory>
#include <string>

#include <base/location.h>
#include <base/macros.h>

namespace brillo {
class Error;
using ErrorPtr = std::unique_ptr<Error>;
}  // namespace brillo

namespace shill {

class Error {
 public:
  enum Type {
    kSuccess = 0,      // No error.
    kOperationFailed,  // failure, otherwise unspecified
    kAlreadyConnected,
    kAlreadyExists,
    kIncorrectPin,
    kInProgress,
    kInternalError,
    kInvalidApn,
    kInvalidArguments,
    kInvalidNetworkName,
    kInvalidPassphrase,
    kInvalidProperty,
    kNoCarrier,
    kNotConnected,
    kNotFound,
    kNotImplemented,
    kNotOnHomeNetwork,
    kNotRegistered,
    kNotSupported,
    kOperationAborted,
    kOperationInitiated,
    kOperationTimeout,
    kPassphraseRequired,
    kPermissionDenied,
    kPinBlocked,
    kPinRequired,
    kWrongState,
    kNumErrors
  };

  Error();                    // Success by default.
  explicit Error(Type type);  // Uses the default message for |type|.
  Error(Type type, const std::string& message);
  ~Error();

  void Populate(Type type);  // Uses the default message for |type|.
  void Populate(Type type, const std::string& message);
  void Populate(Type type,
                const std::string& message,
                const base::Location& location);

  void Reset();

  void CopyFrom(const Error& error);

  // Sets the Chromeos |error| and returns true if Error represents failure.
  // Leaves error unchanged, and returns false otherwise.
  bool ToChromeosError(brillo::ErrorPtr* error) const;

  Type type() const { return type_; }
  const std::string& message() const { return message_; }

  bool IsSuccess() const { return type_ == kSuccess; }
  bool IsFailure() const { return !IsSuccess() && !IsOngoing(); }
  bool IsOngoing() const { return type_ == kOperationInitiated; }

  static std::string GetDBusResult(Type type);
  static std::string GetDefaultMessage(Type type);

  // Log an error message from |from_here|.  If |error| is non-NULL, also
  // populate it.
  static void PopulateAndLog(const base::Location& from_here,
                             Error* error,
                             Type type,
                             const std::string& message);

 private:
  Type type_;
  std::string message_;
  base::Location location_;

  DISALLOW_COPY_AND_ASSIGN(Error);
};

// stream operator provided to facilitate logging
std::ostream& operator<<(std::ostream& stream, const shill::Error& error);

}  // namespace shill

#endif  // SHILL_ERROR_H_
