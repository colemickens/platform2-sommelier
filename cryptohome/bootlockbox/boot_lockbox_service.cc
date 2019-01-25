// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/bootlockbox/boot_lockbox_service.h"

#include <sysexits.h>

#include <base/logging.h>
#include <dbus/dbus-protocol.h>

#include "cryptohome/bootlockbox/tpm2_nvspace_utility.h"
#include "cryptohome/bootlockbox/tpm_nvspace_interface.h"
#include "cryptohome/crypto.h"
#include "cryptohome/cryptolib.h"
#include "cryptohome/platform.h"
#include "cryptohome/tpm.h"
#include "cryptohome/tpm_init.h"

namespace {
// This is a exit status returned when nvspace cannot be created.
// Nvspace can only be created first boot so this exit value is normal.
// Must be in sync with bootlockboxd.conf on the normal exit status value.
constexpr int EX_NVSPACE_NOT_AVAILABLE = 100;
}

namespace cryptohome {

int BootLockboxService::OnInit() {
  nvspace_utility_ = std::make_unique<TPM2NVSpaceUtility>();
  if (!nvspace_utility_->Initialize()) {
    LOG(ERROR) << "Failed to initialize nvspace utility";
    return EX_UNAVAILABLE;
  }
  boot_lockbox_.reset(new NVRamBootLockbox(nvspace_utility_.get()));

  if (!boot_lockbox_->Load() &&
      boot_lockbox_->GetState() == NVSpaceState::kNVSpaceUndefined ) {
    LOG(INFO) << "NVSpace is not defined, define it now";
    if (!boot_lockbox_->DefineSpace()) {
      LOG(ERROR) << "Failed to create nvspace";
      return EX_NVSPACE_NOT_AVAILABLE;
    }
  }
  const int return_code = brillo::DBusServiceDaemon::OnInit();
  if (return_code != EX_OK) {
    LOG(ERROR) << "Failed to start bootlockbox service";
    return return_code;
  }
  LOG(INFO) << "BootLockboxd started";
  return EX_OK;
}

void BootLockboxService::OnShutdown(int *exit_code) {
  VLOG(1) << "Shutting down bootlockbox service";
  brillo::DBusServiceDaemon::OnShutdown(exit_code);
}

void BootLockboxService::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  VLOG(1) << "Register dbus objects...";
  boot_lockbox_dbus_adaptor_.reset(new BootLockboxDBusAdaptor(bus_,
      boot_lockbox_.get()));
  boot_lockbox_dbus_adaptor_->RegisterAsync(
      sequencer->GetHandler("RegisterAsync() failed", true));
  VLOG(1) << "Register dbus object complete";
}

BootLockboxService::BootLockboxService()
    : brillo::DBusServiceDaemon("org.chromium.BootLockbox") {}

BootLockboxService::~BootLockboxService() {}

}  // namespace cryptohome
