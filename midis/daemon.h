// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_DAEMON_H_
#define MIDIS_DAEMON_H_

#include <memory>

#include <base/memory/weak_ptr.h>
#include <brillo/daemons/daemon.h>
#include <dbus/exported_object.h>

#include "midis/client_tracker.h"
#include "midis/device_tracker.h"

namespace dbus {

class MethodCall;

}  // namespace dbus

namespace midis {

class Daemon : public brillo::Daemon {
 public:
  Daemon();
  ~Daemon() override;

 protected:
  int OnInit() override;

 private:
  // This function initializes the D-Bus service. The primary function of the
  // D-Bus interface is to receive a FD from the Chrome process so that we can
  // bootstrap a Mojo IPC channel. Since we should expect requests for client
  // registration to occur as soon as the D-Bus channel is up, this
  // initialization should be the last thing that happens in Daemon::OnInit().
  void InitDBus();

  // Handles BootstrapMojoConnection D-Bus method calls.
  void BootstrapMojoConnection(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  std::unique_ptr<DeviceTracker> device_tracker_;
  std::unique_ptr<ClientTracker> client_tracker_;

  base::WeakPtrFactory<Daemon> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};
}  // namespace midis
#endif  // MIDIS_DAEMON_H_
