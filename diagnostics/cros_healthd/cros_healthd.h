// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_H_

#include <memory>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/dbus/dbus_object.h>

#include "debugd/dbus-proxies.h"
#include "diagnostics/cros_healthd/cros_healthd_mojo_service.h"
#include "diagnostics/cros_healthd/utils/battery_utils.h"
#include "mojo/cros_healthd.mojom.h"

namespace diagnostics {

// Daemon class for cros_healthd.
class CrosHealthd final : public brillo::DBusServiceDaemon {
 public:
  explicit CrosHealthd(std::unique_ptr<org::chromium::debugdProxy> proxy);
  ~CrosHealthd() override;

 private:
  // brillo::DBusServiceDaemon overrides:
  int OnInit() override;
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override;
  void OnShutdown(int* error_code) override;

  // Implementation of the "org.chromium.CrosHealthdInterface" D-Bus interface
  // exposed by the cros_healthd daemon (see constants for the API methods at
  // src/platform2/system_api/dbus/cros_healthd/dbus-constants.h). When
  // |is_chrome| = false, this method will return a unique token that can be
  // used to connect to cros_healthd via mojo. When |is_chrome| = true, the
  // returned string has no meaning.
  std::string BootstrapMojoConnection(const base::ScopedFD& mojo_fd,
                                      bool is_chrome);

  void ShutDownDueToMojoError(const std::string& debug_reason);

  std::unique_ptr<org::chromium::debugdProxy> proxy_;
  std::unique_ptr<BatteryFetcher> battery_fetcher_;
  std::unique_ptr<CrosHealthdMojoService> mojo_service_;

  // Connects BootstrapMojoConnection with the methods of the D-Bus object
  // exposed by the cros_healthd daemon.
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(CrosHealthd);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_H_
