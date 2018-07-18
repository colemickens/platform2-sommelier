// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_STATUS_H_
#define PORTIER_STATUS_H_

// Standard C++ Library.
#include <ostream>
#include <string>

namespace portier {

// Used to return error status of method call.
class Status {
 public:
  // Status code.  Use kOK for all success operators.
  // In general, it is not expected that an OK status will have a
  // message.  These codes are specific to the needs of Portier.
  enum class Code {
    // Pass.
    OK,
    // Failures.
    BAD_PERMISSIONS,
    DOES_NOT_EXIST,
    RESULT_UNAVAILABLE,
    UNEXPECTED_FAILURE,
    INVALID_ARGUMENT,
    MTU_EXCEEDED,
    MALFORMED_PACKET,
    RESOURCE_IN_USE,
    UNSUPPORTED_TYPE,
    // Errors.
    BAD_INTERNAL_STATE
  };

  static std::string GetCodeName(Code code);

  Status();
  explicit Status(Code code);
  Status(Code code, const std::string& message);

  Status(const Status& other) = default;

  // Move constructor and operator.  These operators have special
  // function with respect to the status message.  In the event that
  // a status object is returned from a function, and a new message
  // is inserted into the object, any messages that were inserted
  // before the return will be inserted at the end of the message.
  // This is to allow the message strings to be printed in increasing
  // order of technical information.
  Status(Status&& other);
  Status& operator=(Status&& other);

  Code code() const;

  // Returns the full string representation of the status.
  //  General format: <code name>: <message>[: <sub_message>]
  std::string ToString() const;

  // Boolean operator returns true if the status is OK and false otherwise.
  explicit operator bool() const;
  bool IsOK() const;

  // Used to concatenate to the message buffer.
  Status& operator<<(const std::string& message);

 private:
  // Status code.
  Code code_;
  // String containing message from "current level".
  std::string message_;
  // Sub-message.  The part of the status message that was passed
  // back to calling functions.
  std::string sub_message_;
};

// Output status insertion operator.
std::ostream& operator<<(std::ostream& os, const Status& status);

// Convenience macro for returning on failure.  The macro is intentionally
// defined without a trailing semi-colon to allow extending the message.
// Ex:
//    PORTIER_RETURN_ON_FAILURE(my_status) << "Failed to initialize";
#define PORTIER_RETURN_ON_FAILURE(status) \
  if (!status.IsOK())                     \
  return status

}  // namespace portier

#endif  // PORTIER_STATUS_H_
