// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Standard C++ Library.
#include <utility>

// Chrome OS Library.
#include <base/strings/stringprintf.h>

// Portier Library.
#include "portier/status.h"

namespace portier {

using std::string;
using std::ostream;

using base::StringPrintf;

using Code = Status::Code;

string Status::GetCodeName(Code code) {
  switch (code) {
    case Code::OK:
      return "OK";
    case Code::BAD_PERMISSIONS:
      return "Bad Permissions";
    case Code::DOES_NOT_EXIST:
      return "Does Not Exist";
    case Code::RESULT_UNAVAILABLE:
      return "Result Unavailable";
    case Code::UNEXPECTED_FAILURE:
      return "Unexpected Failure";
    case Code::INVALID_ARGUMENT:
      return "Invalid Argument";
    case Code::MTU_EXCEEDED:
      return "MTU Exceeded";
    case Code::MALFORMED_PACKET:
      return "Malformed Packet";
    case Code::RESOURCE_IN_USE:
      return "Resource In Use";
    case Code::UNSUPPORTED_TYPE:
      return "Unsupported Type";
    case Code::BAD_INTERNAL_STATE:
      return "Bad Internal State";
  }
  return base::StringPrintf("Unknown (%d)", static_cast<int>(code));
}

Status::Status() : code_(Code::OK) {}

Status::Status(Code code) : code_(code) {}

Status::Status(Code code, const string& message)
    : code_(code), message_(message) {}

Status::Status(Status&& other) : code_(other.code_) {
  if (other.message_.size() > 0 && other.sub_message_.size() > 0) {
    sub_message_ = StringPrintf("%s: %s", other.message_.c_str(),
                                other.sub_message_.c_str());
    other.message_.clear();
    other.sub_message_.clear();
  } else if (other.message_.size() > 0) {
    sub_message_ = std::move(other.message_);
  } else if (other.sub_message_.size() > 0) {
    sub_message_ = std::move(other.sub_message_);
  }
}

Status& Status::operator=(Status&& other) {
  code_ = other.code_;
  message_.clear();

  if (other.message_.size() > 0 && other.sub_message_.size() > 0) {
    sub_message_ = StringPrintf("%s: %s", other.message_.c_str(),
                                other.sub_message_.c_str());
    other.sub_message_.clear();
  } else if (other.message_.size() > 0) {
    sub_message_ = std::move(other.message_);
  } else if (other.sub_message_.size() > 0) {
    sub_message_ = std::move(other.sub_message_);
  } else {
    sub_message_.clear();
  }

  return *this;
}

Code Status::code() const {
  return code_;
}

string Status::ToString() const {
  if (message_.size() > 0 && sub_message_.size() > 0) {
    return StringPrintf("%s: %s: %s", GetCodeName(code()).c_str(),
                        message_.c_str(), sub_message_.c_str());
  } else if (message_.size() > 0) {
    return StringPrintf("%s: %s", GetCodeName(code()).c_str(),
                        message_.c_str());
  } else if (sub_message_.size() > 0) {
    return StringPrintf("%s: %s", GetCodeName(code()).c_str(),
                        sub_message_.c_str());
  }
  return GetCodeName(code());
}

Status::operator bool() const {
  return IsOK();
}

bool Status::IsOK() const {
  return code() == Code::OK;
}

Status& Status::operator<<(const string& message) {
  message_.append(message);
  return *this;
}

ostream& operator<<(ostream& os, const Status& status) {
  os << status.ToString();
  return os;
}

}  // namespace portier
