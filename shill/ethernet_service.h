// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_SERVICE_
#define SHILL_ETHERNET_SERVICE_

#include <base/basictypes.h>

#include "shill/refptr_types.h"
#include "shill/shill_event.h"
#include "shill/service.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;

class EthernetService : public Service {
 public:
  EthernetService(ControlInterface *control_interface,
                  EventDispatcher *dispatcher,
                  Manager *manager,
                  const EthernetRefPtr &device);
  ~EthernetService();
  void Connect();
  void Disconnect();

 protected:
  virtual std::string CalculateState() { return "idle"; }

 private:
  std::string GetDeviceRpcId();

  EthernetRefPtr ethernet_;
  const std::string type_;
  DISALLOW_COPY_AND_ASSIGN(EthernetService);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_SERVICE_
