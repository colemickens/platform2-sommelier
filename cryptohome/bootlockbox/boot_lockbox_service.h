// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_SERVICE_H_
#define CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_SERVICE_H_

#include <memory>

#include <brillo/daemons/dbus_daemon.h>

#include "cryptohome/bootlockbox/boot_lockbox.h"
#include "cryptohome/bootlockbox/boot_lockbox_dbus_adaptor.h"

namespace cryptohome {

class Crypto;
class Platform;
class Tpm;
class TpmInit;

// BootLockboxService that implements the top level setups of bootlockboxd.
class BootLockboxService : public brillo::DBusServiceDaemon {
 public:
  BootLockboxService();
  ~BootLockboxService();

 protected:
  int OnInit() override;
  void OnShutdown(int *exit_code) override;
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override;

 private:
  std::unique_ptr<Platform> platform_;
  std::unique_ptr<Crypto> crypto_;
  std::unique_ptr<BootLockbox> boot_lockbox_;
  std::unique_ptr<BootLockboxDBusAdaptor> boot_lockbox_dbus_adaptor;

  DISALLOW_COPY_AND_ASSIGN(BootLockboxService);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_SERVICE_H_
