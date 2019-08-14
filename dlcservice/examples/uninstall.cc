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

    // Call dlcservice Uninstall API to uninstall a DLC module.
    if (!dlc_service_proxy_->Uninstall(dlc_id_to_uninstall_, &error)) {
      // Uninstall failed due to errors.
      LOG(ERROR) << error->GetCode() << ":" << error->GetMessage();
      return EX_SOFTWARE;
    }
    // Uninstall completed successfully.
    return EX_OK;
  }

  const std::string dlc_id_to_uninstall_ = "dummy-dlc";

  std::unique_ptr<org::chromium::DlcServiceInterfaceProxy> dlc_service_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ExampleDaemon);
};

int main(int argc, const char** argv) {
  ExampleDaemon daemon;
  return daemon.Run();
}
