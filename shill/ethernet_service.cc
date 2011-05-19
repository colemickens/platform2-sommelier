// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>
#include <netinet/ether.h>
#include <linux/if.h>

#include <string>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/manager.h"
#include "shill/shill_event.h"

#include "shill/ethernet.h"
#include "shill/ethernet_service.h"

using std::string;

namespace shill {
EthernetService::EthernetService(ControlInterface *control_interface,
                                 EventDispatcher *dispatcher,
                                 Ethernet *device)
    : Service(control_interface, dispatcher, device),
      ethernet_(device) {
}

EthernetService::~EthernetService() { }

void EthernetService::Connect() { }

void EthernetService::Disconnect() { }

}  // namespace shill
