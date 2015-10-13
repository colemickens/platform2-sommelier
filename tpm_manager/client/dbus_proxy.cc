//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "tpm_manager/client/dbus_proxy.h"

#include <brillo/bind_lambda.h>
#include <brillo/dbus/dbus_method_invoker.h>

#include "tpm_manager/common/dbus_interface.h"

namespace {

// Use a two minute timeout because TPM operations can take a long time.
const int kDBusTimeoutMS = 2 * 60 * 1000;

}  // namespace

namespace tpm_manager {

DBusProxy::DBusProxy() {}

DBusProxy::~DBusProxy() {
  if (bus_) {
    bus_->ShutdownAndBlock();
  }
}

bool DBusProxy::Initialize() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  object_proxy_ = bus_->GetObjectProxy(
      tpm_manager::kTpmManagerServiceName,
      dbus::ObjectPath(tpm_manager::kTpmManagerServicePath));
  return (object_proxy_ != nullptr);
}

void DBusProxy::GetTpmStatus(const GetTpmStatusRequest& request,
                             const GetTpmStatusCallback& callback) {
  CallMethod<GetTpmStatusReply>(tpm_manager::kGetTpmStatus, request, callback);
}

void DBusProxy::TakeOwnership(const TakeOwnershipRequest& request,
                              const TakeOwnershipCallback& callback) {
  CallMethod<TakeOwnershipReply>(
      tpm_manager::kTakeOwnership, request, callback);
}

void DBusProxy::DefineNvram(const DefineNvramRequest& request,
                            const DefineNvramCallback& callback) {
  CallMethod<DefineNvramReply>(tpm_manager::kDefineNvram, request, callback);
}

void DBusProxy::DestroyNvram(const DestroyNvramRequest& request,
                             const DestroyNvramCallback& callback) {
  CallMethod<DestroyNvramReply>(tpm_manager::kDestroyNvram, request, callback);
}

void DBusProxy::WriteNvram(const WriteNvramRequest& request,
                           const WriteNvramCallback& callback) {
  CallMethod<WriteNvramReply>(tpm_manager::kWriteNvram, request, callback);
}

void DBusProxy::ReadNvram(const ReadNvramRequest& request,
                          const ReadNvramCallback& callback) {
  CallMethod<ReadNvramReply>(tpm_manager::kReadNvram, request, callback);
}

void DBusProxy::IsNvramDefined(const IsNvramDefinedRequest& request,
                               const IsNvramDefinedCallback& callback) {
  CallMethod<IsNvramDefinedReply>(
      tpm_manager::kIsNvramDefined, request, callback);
}

void DBusProxy::IsNvramLocked(const IsNvramLockedRequest& request,
                              const IsNvramLockedCallback& callback) {
  CallMethod<IsNvramLockedReply>(
      tpm_manager::kIsNvramLocked, request, callback);
}

void DBusProxy::GetNvramSize(const GetNvramSizeRequest& request,
                             const GetNvramSizeCallback& callback) {
  CallMethod<GetNvramSizeReply>(tpm_manager::kGetNvramSize, request, callback);
}

template<typename ReplyProtobufType,
         typename RequestProtobufType,
         typename CallbackType>
void DBusProxy::CallMethod(const std::string& method_name,
                           const RequestProtobufType& request,
                           const CallbackType& callback) {
  auto on_error = [callback](brillo::Error* error) {
    ReplyProtobufType reply;
    reply.set_status(STATUS_NOT_AVAILABLE);
    callback.Run(reply);
  };
  brillo::dbus_utils::CallMethodWithTimeout(
      kDBusTimeoutMS,
      object_proxy_,
      tpm_manager::kTpmManagerInterface,
      method_name,
      callback,
      base::Bind(on_error),
      request);
}

}  // namespace tpm_manager
