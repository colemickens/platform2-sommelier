// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/daemon.h"

#include <sysexits.h>

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>

#include "modemfwd/component.h"
#include "modemfwd/firmware_directory.h"
#include "modemfwd/modem.h"
#include "modemfwd/modem_flasher.h"
#include "modemfwd/modem_helper_directory.h"
#include "modemfwd/modem_tracker.h"

namespace modemfwd {

Daemon::Daemon(const std::string& helper_directory)
    : Daemon(helper_directory, "") {}

Daemon::Daemon(const std::string& helper_directory,
               const std::string& firmware_directory)
    : helper_dir_path_(helper_directory),
      firmware_dir_path_(firmware_directory),
      weak_ptr_factory_(this) {}

int Daemon::OnInit() {
  int exit_code = brillo::DBusDaemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;

  DCHECK(!helper_dir_path_.empty());

  if (firmware_dir_path_.empty()) {
    // Set up the component and get the sub-directories.
    component_ = Component::Load(bus_);
    if (!component_)
      return EX_UNAVAILABLE;

    firmware_dir_path_ = component_->GetPath();
  }

  if (!base::DirectoryExists(helper_dir_path_)) {
    LOG(ERROR) << "Supplied modem-specific helper directory "
               << helper_dir_path_.value() << " does not exist";
    return EX_UNAVAILABLE;
  }

  helper_directory_ = CreateModemHelperDirectory(helper_dir_path_);
  if (!helper_directory_) {
    LOG(ERROR) << "No suitable helpers found in " << helper_dir_path_.value();
    return EX_UNAVAILABLE;
  }

  if (!base::DirectoryExists(firmware_dir_path_)) {
    LOG(ERROR) << "Supplied firmware directory " << firmware_dir_path_.value()
               << " does not exist";
    return EX_UNAVAILABLE;
  }

  modem_flasher_ = std::make_unique<modemfwd::ModemFlasher>(
      CreateFirmwareDirectory(firmware_dir_path_));

  modem_tracker_ = std::make_unique<modemfwd::ModemTracker>(
      bus_,
      base::Bind(&Daemon::OnModemAppeared, weak_ptr_factory_.GetWeakPtr()));

  return EX_OK;
}

void Daemon::OnModemAppeared(
    std::unique_ptr<org::chromium::flimflam::DeviceProxy> device) {
  auto modem = CreateModem(std::move(device), helper_directory_.get());
  if (!modem)
    return;

  DLOG(INFO) << "Modem appeared with equipment ID \"" << modem->GetEquipmentId()
             << "\"";
  modem_flasher_->TryFlash(modem.get());
}

}  // namespace modemfwd
