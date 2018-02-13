// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ml/daemon.h"

namespace ml {

Daemon::Daemon() {}

Daemon::~Daemon() {}

int Daemon::OnInit() {
  InitDBus();
  return 0;
}

void Daemon::InitDBus() {
  // TODO(kennetht): Register the Mojo connection method for the ML service with
  // D-Bus.
}

void Daemon::BootstrapMojoConnection(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  // TODO(kennetht): Initialize the Mojo connection using the file descriptor
  // for the IPC pipe sent with the D-Bus request.
}

}  // namespace ml
