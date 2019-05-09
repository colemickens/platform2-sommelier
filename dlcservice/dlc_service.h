// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DLCSERVICE_DLC_SERVICE_H_
#define DLCSERVICE_DLC_SERVICE_H_

#include <memory>
#include <string>

#include <base/cancelable_callback.h>
#include <brillo/daemons/dbus_daemon.h>

#include "dlcservice/dlc_service_dbus_adaptor.h"

namespace dlcservice {

// DlcService is a D-Bus service daemon.
class DlcService : public brillo::DBusServiceDaemon {
 public:
  explicit DlcService(bool load_installed);
  ~DlcService() override = default;

 private:
  // brillo::Daemon overrides:
  int OnInit() override;
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override;

  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  std::unique_ptr<dlcservice::DlcServiceDBusAdaptor> dbus_adaptor_;
  // Whether to load installed DLC modules.
  const bool load_installed_;

  DISALLOW_COPY_AND_ASSIGN(DlcService);
};

}  // namespace dlcservice

#endif  // DLCSERVICE_DLC_SERVICE_H_
