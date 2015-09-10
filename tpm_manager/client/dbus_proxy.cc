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

#include <chromeos/bind_lambda.h>
#include <chromeos/dbus/dbus_method_invoker.h>

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
  auto on_error = [callback](chromeos::Error* error) {
    GetTpmStatusReply reply;
    reply.set_status(STATUS_NOT_AVAILABLE);
    callback.Run(reply);
  };
  chromeos::dbus_utils::CallMethodWithTimeout(
      kDBusTimeoutMS,
      object_proxy_,
      tpm_manager::kTpmManagerInterface,
      tpm_manager::kGetTpmStatus,
      callback,
      base::Bind(on_error),
      request);
}

void DBusProxy::TakeOwnership(const TakeOwnershipRequest& request,
                              const TakeOwnershipCallback& callback) {
  auto on_error = [callback](chromeos::Error* error) {
    TakeOwnershipReply reply;
    reply.set_status(STATUS_NOT_AVAILABLE);
    callback.Run(reply);
  };
  chromeos::dbus_utils::CallMethodWithTimeout(
      kDBusTimeoutMS,
      object_proxy_,
      tpm_manager::kTpmManagerInterface,
      tpm_manager::kTakeOwnership,
      callback,
      base::Bind(on_error),
      request);
}

}  // namespace tpm_manager
