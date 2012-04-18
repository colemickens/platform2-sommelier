// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/virtio_ethernet.h"

#include <unistd.h>

#include <string>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/scope_logger.h"

using std::string;

namespace shill {

VirtioEthernet::VirtioEthernet(ControlInterface *control_interface,
                               EventDispatcher *dispatcher,
                               Metrics *metrics,
                               Manager *manager,
                               const string &link_name,
                               const string &address,
                               int interface_index)
    : Ethernet(control_interface,
               dispatcher,
               metrics,
               manager,
               link_name,
               address,
               interface_index) {
  SLOG(Ethernet, 2) << "VirtioEthernet device " << link_name << " initialized.";
}

VirtioEthernet::~VirtioEthernet() {
  // Nothing to be done beyond what Ethernet dtor does.
}

void VirtioEthernet::Start(Error *error,
                           const EnabledStateChangedCallback &callback) {
  // We are sometimes instantiated (by DeviceInfo) before the Linux kernel
  // has completed the setup function for the device (virtio_net:virtnet_probe).
  //
  // Furthermore, setting the IFF_UP flag on the device (as done in
  // Ethernet::Start) may cause the kernel IPv6 code to send packets even
  // though virtnet_probe has not completed.
  //
  // When that happens, the device gets stuck in a state where it cannot
  // transmit any frames. (See crosbug.com/29494)
  //
  // To avoid this, we sleep to let the device setup function complete.
  SLOG(Ethernet, 2) << "Sleeping to let virtio initialize.";
  sleep(2);
  SLOG(Ethernet, 2) << "Starting virtio Ethernet.";
  Ethernet::Start(error, callback);
}

}  // namespace shill
