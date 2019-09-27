// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_U2F_DAEMON_H_
#define U2FD_U2F_DAEMON_H_

#include <memory>
#include <string>

#include <brillo/daemons/daemon.h>
#include <brillo/dbus/dbus_object.h>
#include <brillo/dbus/dbus_signal.h>
#include <metrics/metrics_library.h>
#include <power_manager-client/power_manager/dbus-proxies.h>
#include <session_manager/dbus-proxies.h>
#include <u2f/proto_bindings/interface.pb.h>

#include "u2fd/tpm_vendor_cmd.h"
#include "u2fd/u2f_msg_handler.h"
#include "u2fd/u2fhid.h"

namespace u2f {

enum class U2fMode : uint8_t;

// U2F Daemon; starts/runs the virtual USB HID U2F device, and implements the
// U2F DBus interface.
class U2fDaemon : public brillo::Daemon {
 public:
  U2fDaemon(bool force_u2f,
            bool force_g2f,
            bool legacy_kh_fallback,
            uint32_t vendor_id,
            uint32_t product_id);

 private:
  int OnInit() override;

  // Checks if the device policy is available, and if so, starts the U2F
  // service.
  void TryStartService(const std::string& /* unused dbus signal status */);

  // Starts the service. Calling after the service is started is a no-op.
  // Returns:
  //   EX_OK on success
  //   EX_CONFIG if the service is disabled (by flags and/or policy)
  //   EX_PROTOCOL if the cr50 version is incompatible or virtual HID device
  //   cannot be initialized EX_IOERR if DBus cannot be initialized
  int StartService();

  // Creates DBus connection and takes ownership of the U2F DBus service.
  bool InitializeDBus();

  // Initializes DBus proxies for PowerManager, SessionManager, and Trunks.
  bool InitializeDBusProxies();

  // Registers the U2F interface.
  void RegisterDBusU2fInterface();

  // Sends a DBus signal that indicates to Chrome a 'Press Power Button'
  // notification should be displayed.
  void SendWinkSignal();

  // Calls PowerManager to request that power button presses be ignored for a
  // short time.
  void IgnorePowerButtonPress();

  // U2F Behavior Flags
  const bool force_u2f_;
  const bool force_g2f_;
  const bool legacy_kh_fallback_;
  U2fMode u2f_mode_;

  // Virtual USB Device ID
  uint32_t vendor_id_;
  uint32_t product_id_;

  // DBus
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  // Signal sent by this daemon
  std::weak_ptr<brillo::dbus_utils::DBusSignal<u2f::UserNotification>>
      wink_signal_;

  // Proxies to call other daemons
  u2f::TpmVendorCommandProxy tpm_proxy_;
  std::unique_ptr<org::chromium::PowerManagerProxy> pm_proxy_;
  std::unique_ptr<org::chromium::SessionManagerInterfaceProxy> sm_proxy_;

  // Virtual USB Device
  std::unique_ptr<u2f::U2fHid> u2fhid_;
  std::unique_ptr<u2f::U2fMessageHandler> u2f_msg_handler_;

  // UMA, used by Virtual USB Device
  MetricsLibrary metrics_library_;

  DISALLOW_COPY_AND_ASSIGN(U2fDaemon);
};

}  // namespace u2f

#endif  // U2FD_U2F_DAEMON_H_
