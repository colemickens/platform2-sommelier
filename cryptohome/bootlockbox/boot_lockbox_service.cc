// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/bootlockbox/boot_lockbox_service.h"

#include <sysexits.h>

#include <base/logging.h>
#include <dbus/dbus-protocol.h>

#include "cryptohome/crypto.h"
#include "cryptohome/cryptolib.h"
#include "cryptohome/platform.h"
#include "cryptohome/tpm.h"
#include "cryptohome/tpm_init.h"

namespace cryptohome {

int BootLockboxService::OnInit() {
  VLOG(1) << "Starting bootlockbox service";
  const int return_code = brillo::DBusServiceDaemon::OnInit();
  if (return_code != EX_OK) {
    LOG(ERROR) << "Failed to start bootlockbox service";
    return return_code;
  }
  LOG(INFO) << "BootLockbox service started";
  return EX_OK;
}

void BootLockboxService::OnShutdown(int *exit_code) {
  VLOG(1) << "Shutting down bootlockbox service";
  brillo::DBusServiceDaemon::OnShutdown(exit_code);
}

void BootLockboxService::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  VLOG(1) << "Register dbus objects...";

  platform_.reset(new Platform());
  crypto_.reset(new Crypto(platform_.get()));
  crypto_->set_use_tpm(true);
  crypto_->set_tpm(Tpm::GetSingleton());

  boot_lockbox_.reset(new BootLockbox(Tpm::GetSingleton(), platform_.get(),
      crypto_.get()));
  boot_lockbox_dbus_adaptor.reset(new BootLockboxDBusAdaptor(bus_,
      boot_lockbox_.get()));

  // Try to pre load the key to speed up verify operations if the
  // key is available.
  if (!boot_lockbox_->PreLoadKey()) {
    VLOG(1) << "Failed to pre load key, first time booting?";
  }

  boot_lockbox_dbus_adaptor->RegisterAsync(
      sequencer->GetHandler("RegisterAsync() failed", true));
  VLOG(1) << "Register dbus object complete";
}

BootLockboxService::BootLockboxService()
    : brillo::DBusServiceDaemon("org.chromium.BootLockbox") {}

BootLockboxService::~BootLockboxService() {}

}  // namespace cryptohome
