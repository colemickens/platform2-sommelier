// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/arc_sideload_status.h"

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/callback_helpers.h>
#include <base/files/file_util.h>
#include <brillo/cryptohome.h>
#include <brillo/dbus/dbus_object.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "bootlockbox/proto_bindings/boot_lockbox_rpc.pb.h"
#include "login_manager/dbus_util.h"
#include "login_manager/proto_bindings/arc.pb.h"

namespace login_manager {

namespace {

// Boot attribute used to track if the user has allowed sideloading on the
// device.
constexpr char kSideloadingAllowedBootAttribute[] = "arc_sideloading_allowed";

}  // namespace

ArcSideloadStatus::ArcSideloadStatus(dbus::ObjectProxy* boot_lockbox_proxy)
    : boot_lockbox_proxy_(boot_lockbox_proxy),
      sideload_status_(ADB_SIDELOAD_UNKNOWN),
      weak_ptr_factory_(this) {}

ArcSideloadStatus::~ArcSideloadStatus() {}

void ArcSideloadStatus::Initialize() {
  boot_lockbox_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&ArcSideloadStatus::OnBootLockboxServiceAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool ArcSideloadStatus::IsAdbSideloadAllowed() {
  return sideload_status_ == ADB_SIDELOAD_ALLOWED;
}

void ArcSideloadStatus::EnableAdbSideload(EnableAdbSideloadCallback callback) {
  // Must be called after initialized.
  if (sideload_status_ == ADB_SIDELOAD_UNKNOWN) {
    std::move(callback).Run(false, "D-Bus service not connected");
    return;
  }

  dbus::MethodCall method_call(cryptohome::kBootLockboxInterface,
                               cryptohome::kBootLockboxStoreBootLockbox);

  cryptohome::StoreBootLockboxRequest proto;
  proto.set_key(kSideloadingAllowedBootAttribute);
  proto.set_data("1");
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(proto);

  boot_lockbox_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&ArcSideloadStatus::OnEnableAdbSideloadSet,
                 weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcSideloadStatus::QueryAdbSideload(QueryAdbSideloadCallback callback) {
  if (sideload_status_ != ADB_SIDELOAD_UNKNOWN) {
    // If we know the status, just return it immediately.
    SendQueryAdbSideloadResponse(std::move(callback));
  } else {
    // We don't know the status. Enqueue the callback for later handling when
    // the status becomes known.
    query_arc_sideload_callback_queue_.emplace(std::move(callback));
  }
}

void ArcSideloadStatus::OnBootLockboxServiceAvailable(bool service_available) {
  if (!service_available) {
    LOG(ERROR) << "Failed to listen for cryptohome service start. Continue as "
               << "sideloading is disallowed.";
    SetAdbSideloadStatusAndNotify(ADB_SIDELOAD_DISALLOWED);
    return;
  }

  GetAdbSideloadAllowed();
}

void ArcSideloadStatus::GetAdbSideloadAllowed() {
  dbus::MethodCall method_call(cryptohome::kBootLockboxInterface,
                               cryptohome::kBootLockboxReadBootLockbox);

  cryptohome::ReadBootLockboxRequest proto;
  proto.set_key(kSideloadingAllowedBootAttribute);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(proto);

  boot_lockbox_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&ArcSideloadStatus::OnGotAdbSideloadAllowed,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool ArcSideloadStatus::ParseResponseFromRead(dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << cryptohome::kBootLockboxInterface << "."
               << cryptohome::kBootLockboxReadBootLockbox << " request failed.";
    return false;
  }

  dbus::MessageReader reader(response);
  cryptohome::BootLockboxBaseReply base_reply;
  if (!reader.PopArrayOfBytesAsProto(&base_reply)) {
    LOG(ERROR) << cryptohome::kBootLockboxInterface << "."
               << cryptohome::kBootLockboxReadBootLockbox
               << " unable to pop ReadBootLockboxReply proto.";
    return false;
  }

  if (base_reply.has_error()) {
    // When the attribute is unset, defaults to no sideloading.
    if (base_reply.error() == cryptohome::BOOTLOCKBOX_ERROR_MISSING_KEY) {
      return false;
    }

    LOG(ERROR) << cryptohome::kBootLockboxInterface << "."
               << cryptohome::kBootLockboxReadBootLockbox
               << " returned error: " << base_reply.error();
    return false;
  }

  if (!base_reply.HasExtension(cryptohome::ReadBootLockboxReply::reply)) {
    LOG(ERROR) << cryptohome::kBootLockboxInterface << "."
               << cryptohome::kBootLockboxReadBootLockbox
               << " missing reply field in ReadBootLockboxReply.";
    return false;
  }

  cryptohome::ReadBootLockboxReply readbootlockbox_reply =
      base_reply.GetExtension(cryptohome::ReadBootLockboxReply::reply);
  if (!readbootlockbox_reply.has_data()) {
    LOG(ERROR) << cryptohome::kBootLockboxInterface << "."
               << cryptohome::kBootLockboxReadBootLockbox
               << " missing data field in ReadBootLockboxReply.";
    return false;
  }

  std::string arc_sideload_allowed = readbootlockbox_reply.data();
  return arc_sideload_allowed == "1";
}

void ArcSideloadStatus::OnGotAdbSideloadAllowed(dbus::Response* response) {
  bool allowed = ParseResponseFromRead(response);
  if (allowed) {
    SetAdbSideloadStatusAndNotify(ADB_SIDELOAD_ALLOWED);
  } else {
    SetAdbSideloadStatusAndNotify(ADB_SIDELOAD_DISALLOWED);
  }
}

void ArcSideloadStatus::OnEnableAdbSideloadSet(
    EnableAdbSideloadCallback callback, dbus::Response* result) {
  if (!result) {
    std::move(callback).Run(false, "result is null");
    return;
  }

  dbus::MessageReader reader(result);
  cryptohome::BootLockboxBaseReply base_reply;
  if (!reader.PopArrayOfBytesAsProto(&base_reply)) {
    std::move(callback).Run(false, "response is not a BootLockboxBaseReply");
    return;
  }

  if (base_reply.has_error()) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  // Re-read setting from bootlockbox now that it has been stored.
  GetAdbSideloadAllowed();
  std::move(callback).Run(true, nullptr);
}

void ArcSideloadStatus::OverrideAdbSideloadStatusTestOnly(bool allowed) {
  sideload_status_ = allowed ? ADB_SIDELOAD_ALLOWED : ADB_SIDELOAD_DISALLOWED;
}

void ArcSideloadStatus::SetAdbSideloadStatusAndNotify(SideloadStatus status) {
  sideload_status_ = status;

  while (!query_arc_sideload_callback_queue_.empty()) {
    SendQueryAdbSideloadResponse(
        std::move(query_arc_sideload_callback_queue_.front()));
    query_arc_sideload_callback_queue_.pop();
  }
}

void ArcSideloadStatus::SendQueryAdbSideloadResponse(
    QueryAdbSideloadCallback callback) {
  callback.Run(sideload_status_ == ADB_SIDELOAD_ALLOWED);
}

}  // namespace login_manager
