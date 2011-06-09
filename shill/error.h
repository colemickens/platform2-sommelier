// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
    kAlreadyExists,
    kInProgress,
    kInternalError,
    kInvalidArguments,
    kInvalidNetworkName,
    kInvalidPassphrase,
    kInvalidProperty,
    kNotFound,
    kNotSupported,
    kPermissionDenied,
    kPinError,
    kNumErrors
  };

  Error(Type type, const std::string& message);
  virtual ~Error();

  void Populate(Type type, const std::string& message);

  void ToDBusError(::DBus::Error *error);

 private:
  static const char * const kErrorNames[kNumErrors];

  Type type_;
  std::string message_;

  DISALLOW_COPY_AND_ASSIGN(Error);
};

}  // namespace shill

#endif  // SHILL_ERROR_
