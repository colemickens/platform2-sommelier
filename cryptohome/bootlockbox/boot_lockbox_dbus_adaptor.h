// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_DBUS_ADAPTOR_H_
#define CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_DBUS_ADAPTOR_H_

#include <memory>
#include <vector>

#include "cryptohome/bootlockbox/boot_lockbox.h"

#include "dbus_adaptors/org.chromium.BootLockboxInterface.h"

namespace cryptohome {
// Implements DBus BootLockboxInterface.
class BootLockboxDBusAdaptor
    : public org::chromium::BootLockboxInterfaceInterface,
      public org::chromium::BootLockboxInterfaceAdaptor {
 public:
  // Don't own boot_lockbox, it is managed by BootLockboxService.
  explicit BootLockboxDBusAdaptor(scoped_refptr<dbus::Bus> bus,
                                  BootLockbox* boot_lockbox);

  // Registers dbus objects.
  void RegisterAsync(
      const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& cb);

  // Uses BootLockbox key to sign data |in_request| which is a protobuf
  // of type SignBootLockboxRequest defined in rpc.proto. |response| is of type
  // SignBootLockboxReply.
  void SignBootLockbox(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          std::vector<uint8_t>>> response,
      const std::vector<uint8_t>& in_request) override;

  // Verfies a signature contained in |in_request|, which is a protobuf of
  // type VerifyBootLockboxRequest defined in prc.proto. |response| is of type
  // BaseReply.
  void VerifyBootLockbox(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          std::vector<uint8_t>>> response,
      const std::vector<uint8_t>& in_request) override;

  // Finalizes the BootLockbox and locks the signing key. |response| is of type
  // BaseReply.
  void FinalizeBootLockbox(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          std::vector<uint8_t>>> response,
      const std::vector<uint8_t>& in_request) override;

 private:
  BootLockbox* boot_lockbox_;
  brillo::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(BootLockboxDBusAdaptor);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_DBUS_ADAPTOR_H_
