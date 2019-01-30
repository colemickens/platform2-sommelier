// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service.h"

#include <string>

#include <chromeos/constants/imageloader.h>
#include <chromeos/dbus/service_constants.h>
#include <sysexits.h>

#include "dlcservice/boot_device.h"
#include "dlcservice/boot_slot.h"

namespace dlcservice {
namespace {
// Delay for scheduling shutdown event.
// The delay is between dlcservice process being started and the corresponding
// D-Bus method call is called. Not delaying the shutdown in |OnInit| will cause
// the process to stop without D-Bus method being invoked.
constexpr base::TimeDelta kShutdownTimeout = base::TimeDelta::FromSeconds(1);

}  // namespace

// kDlcServiceName is defined in chromeos/dbus/service_constant.h
DlcService::DlcService(bool load_installed)
    : DBusServiceDaemon(kDlcServiceName), load_installed_(load_installed) {}

void DlcService::CancelShutdown() {
  shutdown_callback_.Cancel();
}

void DlcService::ScheduleShutdown() {
  shutdown_callback_.Reset(
      base::Bind(&brillo::Daemon::Quit, base::Unretained(this)));
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, shutdown_callback_.callback(), kShutdownTimeout);
}

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
      base::FilePath(imageloader::kDlcImageRootpath), this);
  dbus_adaptor_->RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("RegisterAsync() failed.", true));
}

int DlcService::OnEventLoopStarted() {
  ScheduleShutdown();

  return Daemon::OnEventLoopStarted();
}

}  // namespace dlcservice
