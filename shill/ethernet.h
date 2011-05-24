// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_
#define SHILL_ETHERNET_

#include <string>

#include "shill/device.h"
#include "shill/shill_event.h"

namespace shill {

// Device superclass.  Individual network interfaces types will inherit from
// this class.
class Ethernet : public Device {
 public:
  explicit Ethernet(ControlInterface *control_interface,
                    EventDispatcher *dispatcher,
                    const std::string& link_name,
                    int interface_index);
  ~Ethernet();
  bool TechnologyIs(Device::Technology type);
 private:
  DISALLOW_COPY_AND_ASSIGN(Ethernet);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_
