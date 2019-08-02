// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/bootlockbox/boot_lockbox_dbus_adaptor.h"

#include <memory>
#include <string>
#include <vector>

#include <base/logging.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <brillo/secure_blob.h>
#include <dbus/dbus-protocol.h>

#include "boot_lockbox_rpc.pb.h"  // NOLINT(build/include)

namespace {
// Creates a dbus error message.
brillo::ErrorPtr CreateError(const std::string& code,
                             const std::string& message) {
  return brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                               code, message);
}

}  // namespace

namespace cryptohome {

BootLockboxDBusAdaptor::BootLockboxDBusAdaptor(scoped_refptr<dbus::Bus> bus,
                                               NVRamBootLockbox* boot_lockbox)
    : org::chromium::BootLockboxInterfaceAdaptor(this),
      boot_lockbox_(boot_lockbox),
      dbus_object_(
          nullptr,
          bus,
          org::chromium::BootLockboxInterfaceAdaptor::GetObjectPath()) {}

void BootLockboxDBusAdaptor::RegisterAsync(
    const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& cb) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(cb);
}

void BootLockboxDBusAdaptor::StoreBootLockbox(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        cryptohome::BootLockboxBaseReply>> response,
    const cryptohome::StoreBootLockboxRequest& in_request) {
  if (!in_request.has_key() || !in_request.has_data()) {
    brillo::ErrorPtr error =
        CreateError(DBUS_ERROR_INVALID_ARGS,
                    "StoreBootLockboxRequest has invalid argument(s).");
    response->ReplyWithError(error.get());
    return;
  }

  cryptohome::BootLockboxBaseReply reply;
  if (!boot_lockbox_->Store(in_request.key(), in_request.data())) {
    reply.set_error(cryptohome::BOOTLOCKBOX_ERROR_CANNOT_STORE);
  }
  response->Return(reply);
}

void BootLockboxDBusAdaptor::ReadBootLockbox(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        cryptohome::BootLockboxBaseReply>> response,
    const cryptohome::ReadBootLockboxRequest& in_request) {
  if (!in_request.has_key()) {
    brillo::ErrorPtr error =
        CreateError(DBUS_ERROR_INVALID_ARGS,
                    "ReadBootLockboxRequest has invalid argument(s).");
    response->ReplyWithError(error.get());
    return;
  }
  cryptohome::BootLockboxBaseReply reply;
  std::string data;
  if (!boot_lockbox_->Read(in_request.key(), &data)) {
    reply.set_error(cryptohome::BOOTLOCKBOX_ERROR_MISSING_KEY);
  } else {
    reply.MutableExtension(cryptohome::ReadBootLockboxReply::reply)
        ->set_data(data);
  }
  response->Return(reply);
}

void BootLockboxDBusAdaptor::FinalizeBootLockbox(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        cryptohome::BootLockboxBaseReply>> response,
    const cryptohome::FinalizeNVRamBootLockboxRequest& in_request) {
  cryptohome::BootLockboxBaseReply reply;
  if (!boot_lockbox_->Finalize()) {
    reply.set_error(cryptohome::BOOTLOCKBOX_ERROR_TPM_COMM_ERROR);
  }
  response->Return(reply);
}

}  // namespace cryptohome
