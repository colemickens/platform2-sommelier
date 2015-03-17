// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <memory>

#include <base/bind.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/daemons/dbus_daemon.h>

#include "brdebug/device_property_watcher.h"
#include "brdebug/peerd_client.h"

namespace brdebug {

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon() {}
  ~Daemon() {}

 protected:
  int OnInit() override {
    int return_code = DBusDaemon::OnInit();
    if (return_code != EX_OK) return return_code;

    device_property_watcher_.reset(
        new DevicePropertyWatcher(
            base::Bind(&Daemon::HandleDevicePropertyChange,
                       weak_ptr_factory_.GetWeakPtr())));
    if (!device_property_watcher_->Init()) {
      return EX_SOFTWARE;
    }

    peerd_client_.reset(
        new PeerdClient(bus_, device_property_watcher_->GetDeviceProperties()));
    return return_code;
  }

 private:
  void HandleDevicePropertyChange() {
    peerd_client_->UpdateServiceInfo(
        device_property_watcher_->GetDeviceProperties());
  }

  std::unique_ptr<DevicePropertyWatcher> device_property_watcher_;
  std::unique_ptr<PeerdClient> peerd_client_;
  base::WeakPtrFactory<Daemon> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace brdebug

int main() {
  brdebug::Daemon daemon;
  LOG(INFO) << "Starting daemon.";
  daemon.Run();
  LOG(INFO) << "Daemon stopped.";
  return 0;
}
