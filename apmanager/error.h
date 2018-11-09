// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_ERROR_H_
#define APMANAGER_ERROR_H_

#include <memory>
#include <string>

#include <base/location.h>
#include <base/macros.h>

namespace brillo {
class Error;
using ErrorPtr = std::unique_ptr<Error>;
}  // namespace brillo

namespace apmanager {

class Error {
 public:
  enum Type {
    kSuccess = 0,  // No error.
    kOperationInProgress,
    kInternalError,
    kInvalidArguments,
    kInvalidConfiguration,
    kNumErrors
  };

  Error();
  ~Error();

  void Populate(Type type,
                const std::string& message,
                const tracked_objects::Location& location);

  void Reset();

  Type type() const { return type_; }
  const std::string& message() const { return message_; }

  bool IsSuccess() const { return type_ == kSuccess; }
  bool IsFailure() const { return !IsSuccess() && !IsOngoing(); }
  bool IsOngoing() const { return type_ == kOperationInProgress; }

  // Log an error message from |from_here|.  If |error| is non-NULL, also
  // populate it.
  static void PopulateAndLog(Error* error,
                             Type type,
                             const std::string& message,
                             const tracked_objects::Location& from_here);

  // TODO(zqiu): put this under a compiler flag (e.g. __DBUS__).
  // Sets the D-Bus error and returns true if Error represents failure.
  // Leaves error unchanged, and returns false otherwise.
  bool ToDBusError(brillo::ErrorPtr* error) const;

 private:
  friend class ErrorTest;

  Type type_;
  std::string message_;
  tracked_objects::Location location_;

  DISALLOW_COPY_AND_ASSIGN(Error);
};

}  // namespace apmanager

#endif  // APMANAGER_ERROR_H_
