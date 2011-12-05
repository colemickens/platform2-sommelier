// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ERROR_
#define SHILL_ERROR_

#include <string>

#include <base/basictypes.h>

namespace DBus {
class Error;
}  // namespace DBus

namespace shill {

class Error {
 public:
  enum Type {
    kSuccess = 0,  // No error.
    kOperationFailed,  // failure, otherwise unspecified
    kAlreadyConnected,
    kAlreadyExists,
    kInProgress,
    kInternalError,
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
    kOperationTimeout,
    kPassphraseRequired,
    kIncorrectPin,
    kPinRequired,
    kPinBlocked,
    kPermissionDenied,
    kNumErrors
  };

  Error();  // Success by default.
  explicit Error(Type type);  // Uses the default message for |type|.
  Error(Type type, const std::string &message);
  ~Error();

  void Populate(Type type);  // Uses the default message for |type|.
  void Populate(Type type, const std::string &message);

  void Reset();

  void CopyFrom(const Error &error);

  // Sets the DBus |error| and returns true if Error represents failure.
  // Leaves |error| unchanged, and returns false, otherwise.
  bool ToDBusError(::DBus::Error *error) const;

  Type type() const { return type_; }
  const std::string &message() const { return message_; }

  bool IsSuccess() const { return type_ == kSuccess; }
  bool IsFailure() const { return !IsSuccess(); }

  static std::string GetName(Type type);
  static std::string GetDefaultMessage(Type type);

  // Log an error message.  If |error| is non-NULL, also populate it.
  static void PopulateAndLog(Error *error, Type type,
                             const std::string &message);

 private:
  struct Info {
    const char *name;  // Error type name.
    const char *message;  // Default Error type message.
  };

  static const Info kInfos[kNumErrors];
  static const char kInterfaceName[];

  Type type_;
  std::string message_;

  DISALLOW_COPY_AND_ASSIGN(Error);
};

}  // namespace shill

// stream operator provided to facilitate logging
std::ostream &operator<<(std::ostream &stream, const shill::Error &error);

#endif  // SHILL_ERROR_
