// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service.h"

#include <string>

#include <chromeos/constants/imageloader.h>
#include <chromeos/dbus/dlcservice/dbus-constants.h>
#include <sysexits.h>

#include "dlcservice/boot_device.h"
#include "dlcservice/boot_slot.h"

namespace dlcservice {

// kDlcServiceServiceName is defined in
// chromeos/dbus/dlcservice/dbus-constants.h
DlcService::DlcService(bool load_installed)
    : DBusServiceDaemon(kDlcServiceServiceName),
      load_installed_(load_installed) {}

int DlcService::OnInit() {
  int return_code = brillo::DBusServiceDaemon::OnInit();
  if (return_code != EX_OK)
    return return_code;

  if (load_installed_) {
    dbus_adaptor_->LoadDlcModuleImages();
  }
  return EX_OK;
}

void DlcService::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  dbus_object_ = std::make_unique<brillo::dbus_utils::DBusObject>(
      nullptr, bus_,
      org::chromium::DlcServiceInterfaceAdaptor::GetObjectPath());
  dbus_adaptor_ = std::make_unique<dlcservice::DlcServiceDBusAdaptor>(
      std::make_unique<org::chromium::ImageLoaderInterfaceProxy>(bus_),
      std::make_unique<org::chromium::UpdateEngineInterfaceProxy>(bus_),
      std::make_unique<BootSlot>(std::make_unique<BootDevice>()),
      base::FilePath(imageloader::kDlcManifestRootpath),
      base::FilePath(imageloader::kDlcImageRootpath));
  dbus_adaptor_->RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("RegisterAsync() failed.", true));
}

}  // namespace dlcservice
