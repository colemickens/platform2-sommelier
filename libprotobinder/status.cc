// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/parcel.h"
#include "libprotobinder/status.h"

#include <base/logging.h>
#include <base/files/file_path.h>

// IDL definition
#include "libprotobinder/binder.pb.h"

namespace protobinder {

const std::map<Status::BinderStatus, std::string> Status::kErrorStrings = {
    {Status::OK, "OK"},
    {Status::APPLICATION_ERROR, "APPLICATION_ERROR"},
    {Status::DEAD_ENDPOINT, "DEAD_ENDPOINT"},
    {Status::UNKNOWN_CODE, "UNKNOWN_CODE"},
    {Status::DRIVER_ERROR, "DRIVER_ERROR"},
    {Status::BAD_PARCEL, "BAD_PARCEL"},
    {Status::FAILED_TRANSACTION, "FAILED_TRANSACTION"},
    {Status::BAD_PROTO, "BAD_PROTO"},
    {Status::ENDPOINT_NOT_SET, "ENDPOINT_NOT_SET"}};

Status::Status(Parcel* parcel) {
  std::string reply_string;
  parcel->ReadString(&reply_string);

  StatusMessage proto;
  proto.ParseFromString(reply_string);
  CHECK(proto.has_binder_status());
  CHECK(proto.has_app_status());
  CHECK(proto.has_error_message());
  CHECK(proto.has_line());
  CHECK(proto.has_file());

  binder_status_ = Status::kErrorStrings.count(
                       static_cast<BinderStatus>(proto.binder_status()))
                       ? static_cast<BinderStatus>(proto.binder_status())
                       : Status::UNEXPECTED_STATUS;

  application_status_ = proto.app_status();
  error_message_ = proto.error_message();
  line_ = proto.line();
  file_ = proto.file();
}

Status::Status(BinderStatus status,
               int application_status,
               const std::string& error_message,
               int line,
               const std::string& file)
    : binder_status_(status),
      application_status_(application_status),
      error_message_(error_message),
      line_(line) {
  file_ = base::FilePath(file).BaseName().value();
}

Status::~Status() {
}

void Status::AddToParcel(Parcel* parcel) const {
  // First add Status to a proto.
  StatusMessage proto;
  proto.set_binder_status(binder_status_);
  proto.set_app_status(application_status_);
  proto.set_error_message(error_message_);
  proto.set_line(line_);
  proto.set_file(file_);

  // Now add that to the Parcel.
  std::string reply_string;
  proto.SerializeToString(&reply_string);
  parcel->WriteString(reply_string);
}

Status Status::BinderError(const BinderStatus status,
                           int line,
                           const std::string& file,
                           bool log,
                           logging::LogSeverity level) {
  Status status_obj(status, 0, "", line, file);
  if (log)
    logging::LogMessage(file.c_str(), line, level).stream() << status_obj;
  return status_obj;
}

Status Status::AppError(int status,
                        const std::string& str,
                        int line,
                        const std::string& file,
                        bool log,
                        logging::LogSeverity level) {
  Status status_obj(APPLICATION_ERROR, status, str, line, file);
  if (log)
    logging::LogMessage(file.c_str(), line, level).stream() << status_obj;
  return status_obj;
}

std::ostream& operator<<(std::ostream& os, const Status& status) {
  if (status.IsOk()) {
    os << "Status: Ok";
  } else if (status.IsAppError()) {
    os << "Status: Application Error " << status.application_status() << " \""
       << status.error_message() << "\"";
  } else {
    os << "Status: Binder Error " << status.status() << " ";
    auto it = Status::kErrorStrings.find(status.status());
    if (it == Status::kErrorStrings.end())
      os << "Unknown Binder error";
    else
      os << it->second;
  }
  os << " [" << status.file() << ":" << status.line() << "]";
  return os;
}

}  // namespace protobinder
