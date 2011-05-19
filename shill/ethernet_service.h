// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_SERVICE_
#define SHILL_ETHERNET_SERVICE_

#include <base/basictypes.h>

#include "shill/ethernet.h"
#include "shill/device.h"
#include "shill/shill_event.h"
#include "shill/service.h"

namespace shill {


class Ethernet;

class EthernetService : public Service {
 public:
  EthernetService(ControlInterface *control_interface,
                  EventDispatcher *dispatcher,
                  Ethernet *device);
  ~EthernetService();
  void Connect();
  void Disconnect();
 private:
  Ethernet *ethernet_;
  DISALLOW_COPY_AND_ASSIGN(EthernetService);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_SERVICE_
