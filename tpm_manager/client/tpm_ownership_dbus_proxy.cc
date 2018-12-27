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

#include "tpm_manager/client/tpm_ownership_dbus_proxy.h"

#include <base/bind.h>
#include <brillo/dbus/dbus_method_invoker.h>

#include "tpm_manager/common/tpm_manager_constants.h"
#include "tpm_manager/common/tpm_ownership_dbus_interface.h"

namespace {

// Use a two minute timeout because TPM operations can take a long time.
const int kDBusTimeoutMS = 2 * 60 * 1000;

}  // namespace

namespace tpm_manager {

TpmOwnershipDBusProxy::~TpmOwnershipDBusProxy() {
  if (bus_) {
    bus_->ShutdownAndBlock();
  }
}

bool TpmOwnershipDBusProxy::Initialize() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  object_proxy_ = bus_->GetObjectProxy(
      tpm_manager::kTpmManagerServiceName,
      dbus::ObjectPath(tpm_manager::kTpmManagerServicePath));
  return (object_proxy_ != nullptr);
}

void TpmOwnershipDBusProxy::GetTpmStatus(const GetTpmStatusRequest& request,
                                         const GetTpmStatusCallback& callback) {
  CallMethod<GetTpmStatusReply>(tpm_manager::kGetTpmStatus, request, callback);
}

void TpmOwnershipDBusProxy::GetDictionaryAttackInfo(
    const GetDictionaryAttackInfoRequest& request,
    const GetDictionaryAttackInfoCallback& callback) {
  CallMethod<GetDictionaryAttackInfoReply>(
      tpm_manager::kGetDictionaryAttackInfo, request, callback);
}

void TpmOwnershipDBusProxy::TakeOwnership(
    const TakeOwnershipRequest& request,
    const TakeOwnershipCallback& callback) {
  CallMethod<TakeOwnershipReply>(tpm_manager::kTakeOwnership, request,
                                 callback);
}

void TpmOwnershipDBusProxy::RemoveOwnerDependency(
    const RemoveOwnerDependencyRequest& request,
    const RemoveOwnerDependencyCallback& callback) {
  CallMethod<RemoveOwnerDependencyReply>(tpm_manager::kRemoveOwnerDependency,
                                         request, callback);
}

void TpmOwnershipDBusProxy::ClearStoredOwnerPassword(
    const ClearStoredOwnerPasswordRequest& request,
    const ClearStoredOwnerPasswordCallback& callback) {
  CallMethod<ClearStoredOwnerPasswordReply>(
      tpm_manager::kClearStoredOwnerPassword, request, callback);
}

template <typename ReplyProtobufType,
          typename RequestProtobufType,
          typename CallbackType>
void TpmOwnershipDBusProxy::CallMethod(const std::string& method_name,
                                       const RequestProtobufType& request,
                                       const CallbackType& callback) {
  auto on_error = [](CallbackType callback, brillo::Error* error) {
    ReplyProtobufType reply;
    reply.set_status(STATUS_NOT_AVAILABLE);
    callback.Run(reply);
  };
  brillo::dbus_utils::CallMethodWithTimeout(
      kDBusTimeoutMS, object_proxy_, tpm_manager::kTpmOwnershipInterface,
      method_name, callback, base::Bind(on_error, callback), request);
}

}  // namespace tpm_manager
