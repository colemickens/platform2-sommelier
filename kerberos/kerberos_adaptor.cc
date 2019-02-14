// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/kerberos_adaptor.h"

#include <string>
#include <utility>

#include <base/compiler_specific.h>
#include <base/optional.h>
#include <brillo/dbus/dbus_object.h>

#include "kerberos/error_strings.h"
#include "kerberos/platform_helper.h"
#include "kerberos/proto_bindings/kerberos_service.pb.h"

namespace kerberos {

namespace {

// Serializes |proto| to a vector of bytes. CHECKs for success (should
// never fail if there are no required proto fields).
std::vector<uint8_t> SerializeProto(
    const google::protobuf::MessageLite& proto) {
  std::vector<uint8_t> proto_blob(proto.ByteSizeLong());
  CHECK(proto.SerializeToArray(proto_blob.data(), proto_blob.size()));
  return proto_blob;
}

// Parses a proto from an array of bytes |proto_blob|. Returns
// ERROR_PARSE_REQUEST_FAILED on error.
WARN_UNUSED_RESULT ErrorType
ParseProto(google::protobuf::MessageLite* proto,
           const std::vector<uint8_t>& proto_blob) {
  if (!proto->ParseFromArray(proto_blob.data(), proto_blob.size())) {
    LOG(ERROR) << "Failed to parse proto";
    return ERROR_PARSE_REQUEST_FAILED;
  }
  return ERROR_NONE;
}

void PrintResult(const char* msg, ErrorType error) {
  if (error == ERROR_NONE)
    LOG(INFO) << msg << " succeeded";
  else
    LOG(INFO) << msg << " failed with " << GetErrorString(error);
}

}  // namespace

KerberosAdaptor::KerberosAdaptor(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object)
    : org::chromium::KerberosAdaptor(this),
      dbus_object_(std::move(dbus_object)) {}

KerberosAdaptor::~KerberosAdaptor() = default;

void KerberosAdaptor::RegisterAsync(
    const brillo::dbus_utils::AsyncEventSequencer::CompletionAction&
        completion_callback) {
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
}

std::vector<uint8_t> KerberosAdaptor::AddAccount(
    const std::vector<uint8_t>& request_blob) {
  LOG(INFO) << "Received AddAccount request";
  AddAccountRequest request;
  ErrorType error = ParseProto(&request, request_blob);

  if (error == ERROR_NONE)
    error = manager_.AddAccount(request.principal_name());

  PrintResult("AddAccount", error);
  AddAccountResponse response;
  response.set_error(error);
  return SerializeProto(response);
}

std::vector<uint8_t> KerberosAdaptor::RemoveAccount(
    const std::vector<uint8_t>& request_blob) {
  LOG(INFO) << "Received RemoveAccount request";
  RemoveAccountRequest request;
  ErrorType error = ParseProto(&request, request_blob);

  if (error == ERROR_NONE)
    error = manager_.RemoveAccount(request.principal_name());

  PrintResult("RemoveAccount", error);
  RemoveAccountResponse response;
  response.set_error(error);
  return SerializeProto(response);
}

std::vector<uint8_t> KerberosAdaptor::AcquireKerberosTgt(
    const std::vector<uint8_t>& request_blob,
    const base::ScopedFD& password_fd) {
  LOG(INFO) << "Received AcquireKerberosTgt request";
  AcquireKerberosTgtRequest request;
  ErrorType error = ParseProto(&request, request_blob);

  base::Optional<std::string> password;
  if (error == ERROR_NONE) {
    password = ReadPipeToString(password_fd.get());
    if (!password.has_value()) {
      LOG(ERROR) << "Failed to read password pipe";
      error = ERROR_LOCAL_IO;
    }
  }

  if (error == ERROR_NONE)
    error = manager_.AcquireTgt(request.principal_name(), password.value());

  PrintResult("AcquireKerberosTgt", error);
  AcquireKerberosTgtResponse response;
  response.set_error(error);
  return SerializeProto(response);
}

std::vector<uint8_t> KerberosAdaptor::GetKerberosFiles(
    const std::vector<uint8_t>& request_blob) {
  LOG(INFO) << "Received GetKerberosFiles request";
  GetKerberosFilesRequest request;
  ErrorType error = ParseProto(&request, request_blob);

  GetKerberosFilesResponse response;
  if (error == ERROR_NONE) {
    error = manager_.GetKerberosFiles(request.principal_name(),
                                      response.mutable_files());
  }

  PrintResult("GetKerberosFiles", error);
  response.set_error(error);
  return SerializeProto(response);
}

}  // namespace kerberos
