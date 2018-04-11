// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ML_DAEMON_H_
#define ML_DAEMON_H_

#include <brillo/daemons/dbus_daemon.h>
#include <dbus/exported_object.h>

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ml {

class Daemon : public brillo::DBusDaemon {
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

  // Handles org.chromium.BootstrapMojoConnection D-Bus method calls.
  void BootstrapMojoConnection(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Must be last class member.
  base::WeakPtrFactory<Daemon> weak_ptr_factory_;
};

}  // namespace ml

#endif  // ML_DAEMON_H_
