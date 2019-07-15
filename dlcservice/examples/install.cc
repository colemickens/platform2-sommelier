// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <string>

#include <brillo/daemons/daemon.h>
#include <brillo/errors/error.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>

#include "dlcservice/dbus-proxies.h"

class ExampleDaemon : public brillo::Daemon {
 public:
  ExampleDaemon() {}

 private:
  bool Init(int* error_ptr) {
    // Initiates dlcservice D-Bus API proxy.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    scoped_refptr<dbus::Bus> bus{new dbus::Bus{options}};
    if (!bus->Connect()) {
      LOG(ERROR) << "Failed to connect to DBus.";
      *error_ptr = EX_UNAVAILABLE;
      return false;
    }
    dlc_service_proxy_ =
        std::make_unique<org::chromium::DlcServiceInterfaceProxy>(bus);

    return true;
  }

  int OnEventLoopStarted() override {
    brillo::ErrorPtr error;
    dlcservice::DlcModuleList dlc_module_list;
    dlcservice::DlcModuleInfo* dlc_module_info =
        dlc_module_list.add_dlc_module_infos();
    dlc_module_info->set_dlc_id(dlc_id_to_install_);

    dlc_service_proxy_->RegisterOnInstalledSignalHandler(
        base::Bind(&ExampleDaemon::OnInstalled, base::Unretained(this)),
        base::Bind(&ExampleDaemon::OnInstalledConnect, base::Unretained(this)));

    // Call dlcservice Install API to install a DLC module.
    if (!dlc_service_proxy_->Install(dlc_module_list, &error)) {
      // Install failed immediately due to errors.
      LOG(ERROR) << error->GetMessage();
      return EX_SOFTWARE;
    }
    return EX_OK;
  }

  void OnInstalled(const dlcservice::InstallResult& install_result) {
    if (!install_result.success()) {
      LOG(ERROR) << "Failed to install with error code:"
                 << install_result.error_code();
    } else {
      LOG(INFO) << "Install successful!";
    }
  }

  void OnInstalledConnect(const std::string& interface_name,
                          const std::string& signal_name,
                          bool success) {
    if (!success)
      QuitWithExitCode(EX_SOFTWARE);
  }

  const std::string dlc_id_to_install_ = "dummy-dlc";

  std::unique_ptr<org::chromium::DlcServiceInterfaceProxy> dlc_service_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ExampleDaemon);
};

int main(int argc, const char** argv) {
  ExampleDaemon daemon;
  return daemon.Run();
}
