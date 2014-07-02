// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VIRTIO_ETHERNET_
#define SHILL_VIRTIO_ETHERNET_

#include <string>

#include "shill/ethernet.h"

namespace shill {

class VirtioEthernet : public Ethernet {
 public:
  VirtioEthernet(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 Metrics *metrics,
                 Manager *manager,
                 const std::string& link_name,
                 const std::string &address,
                 int interface_index);
  virtual ~VirtioEthernet();

  virtual void Start(Error *error, const EnabledStateChangedCallback &callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtioEthernet);
};

}  // namespace shill

#endif  // SHILL_VIRTIO_ETHERNET_
