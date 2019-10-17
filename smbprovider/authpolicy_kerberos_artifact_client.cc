// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/authpolicy_kerberos_artifact_client.h"

#include <memory>
#include <utility>

#include <base/bind.h>
#include <authpolicy/proto_bindings/active_directory_info.pb.h>
#include <dbus/authpolicy/dbus-constants.h>
#include <dbus/message.h>

namespace smbprovider {
namespace {

authpolicy::ErrorType GetErrorFromReader(dbus::MessageReader* reader) {
  int32_t int_error;
  if (!reader->PopInt32(&int_error)) {
    DLOG(ERROR) << "AuthPolicyKerberosArtifactClient: Failed to get an error "
                   "from the response";
    return authpolicy::ERROR_DBUS_FAILURE;
  }
  if (int_error < 0 || int_error >= authpolicy::ERROR_COUNT) {
    return authpolicy::ERROR_UNKNOWN;
  }
  return static_cast<authpolicy::ErrorType>(int_error);
}

authpolicy::ErrorType GetErrorAndProto(
    dbus::Response* response, google::protobuf::MessageLite* protobuf) {
  if (!response) {
    DLOG(ERROR)
        << "AuthPolicyKerberosArtifactClient: Failed to call to authpolicy";
    return authpolicy::ERROR_DBUS_FAILURE;
  }
  dbus::MessageReader reader(response);
  const authpolicy::ErrorType error(GetErrorFromReader(&reader));

  if (error != authpolicy::ERROR_NONE) {
    LOG(ERROR) << "AuthPolicyKerberosArtifactClient: Failed to get Kerberos "
                  "files with error "
               << error;
    return error;
  }

  if (!reader.PopArrayOfBytesAsProto(protobuf)) {
    DLOG(ERROR)
        << "AuthPolicyKerberosArtifactClient: Failed to parse protobuf.";
    return authpolicy::ERROR_DBUS_FAILURE;
  }
  return authpolicy::ERROR_NONE;
}

}  // namespace

AuthPolicyKerberosArtifactClient::AuthPolicyKerberosArtifactClient(
    scoped_refptr<dbus::Bus> bus) {
  authpolicy_object_proxy_ =
      bus->GetObjectProxy(authpolicy::kAuthPolicyServiceName,
                          dbus::ObjectPath(authpolicy::kAuthPolicyServicePath));
}

void AuthPolicyKerberosArtifactClient::GetUserKerberosFiles(
    const std::string& object_guid, GetUserKerberosFilesCallback callback) {
  dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                               authpolicy::kGetUserKerberosFilesMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(object_guid);
  authpolicy_object_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&AuthPolicyKerberosArtifactClient::HandleGetUserKeberosFiles,
                 weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthPolicyKerberosArtifactClient::ConnectToKerberosFilesChangedSignal(
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
  authpolicy_object_proxy_->ConnectToSignal(
      authpolicy::kAuthPolicyInterface,
      authpolicy::kUserKerberosFilesChangedSignal, std::move(signal_callback),
      std::move(on_connected_callback));
}

void AuthPolicyKerberosArtifactClient::HandleGetUserKeberosFiles(
    GetUserKerberosFilesCallback callback, dbus::Response* response) {
  authpolicy::KerberosFiles files_proto;
  bool success =
      (GetErrorAndProto(response, &files_proto) == authpolicy::ERROR_NONE);
  if (success && (!files_proto.has_krb5cc() || !files_proto.has_krb5conf())) {
    DLOG(ERROR)
        << "AuthPolicyKerberosArtifactClient: Kerberos files are empty.";
    success = false;
  }

  callback.Run(success, files_proto.krb5cc(), files_proto.krb5conf());
}

}  // namespace smbprovider
