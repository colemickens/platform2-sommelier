// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
