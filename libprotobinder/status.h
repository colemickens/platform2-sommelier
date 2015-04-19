// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_STATUS_H_
#define LIBPROTOBINDER_STATUS_H_

#include <libgen.h>
#include <string.h>

#include <map>
#include <ostream>
#include <string>

#include <base/logging.h>

#include "binder_export.h"  // NOLINT(build/include)

namespace protobinder {

#define STATUS_OK() Status::Ok(__LINE__, __FILE__)
#define STATUS_BINDER_ERROR(code) \
  Status::BinderError(code, __LINE__, __FILE__, false, 0)
#define STATUS_APP_ERROR(code, message) \
  Status::AppError(code, message, __LINE__, __FILE__, false, 0)
#define STATUS_BINDER_ERROR_LOG(level, code) \
  Status::BinderError(code, __LINE__, __FILE__, true, level)
#define STATUS_APP_ERROR_LOG(level, code, message) \
  Status::AppError(code, message, __LINE__, __FILE__, true, level)

class Parcel;

class BINDER_EXPORT Status {
 public:
  enum BinderStatus {
    OK = 0,
    APPLICATION_ERROR = 1,
    DEAD_ENDPOINT = 2,
    UNKNOWN_CODE = 3,
    DRIVER_ERROR = 4,
    BAD_PARCEL = 5,
    FAILED_TRANSACTION = 6,
    BAD_PROTO = 7,
    ENDPOINT_NOT_SET = 8,
    UNEXPECTED_STATUS = 9
  };

  Status(BinderStatus status,
         int application_status,
         const std::string& error_message,
         int line,
         const std::string& file);
  explicit Status(Parcel* parcel);
  ~Status();

  static Status BinderError(BinderStatus status,
                            int line,
                            const std::string& file,
                            bool log,
                            logging::LogSeverity level);

  static Status Ok(int line, const std::string& file) {
    return BinderError(OK, line, file, false, 0);
  }

  static Status AppError(int status,
                         const std::string& str,
                         int line,
                         const std::string& file,
                         bool log,
                         logging::LogSeverity level);

  bool IsOk() const { return binder_status_ == OK; }

  operator bool() const { return IsOk(); }

  bool IsAppError() const { return binder_status_ == APPLICATION_ERROR; }

  BinderStatus status() const { return binder_status_; }

  int application_status() const {
    return IsAppError() ? application_status_ : 0;
  }

  std::string error_message() const { return error_message_; }

  std::string file() const { return file_; }

  int line() const { return line_; }

  void AddToParcel(Parcel* parcel) const;

  static const std::map<BinderStatus, std::string> kErrorStrings;

 private:
  BinderStatus binder_status_;
  int application_status_;
  std::string error_message_;

  // Where status was created.
  int line_;
  std::string file_;
};

BINDER_EXPORT std::ostream& operator<<(std::ostream& os, const Status& status);

}  // namespace protobinder

#endif  // LIBPROTOBINDER_STATUS_H_
