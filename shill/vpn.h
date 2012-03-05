// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_
#define SHILL_VPN_

#include "shill/device.h"

namespace shill {

class VPN : public Device {
 public:
  VPN(ControlInterface *control,
      EventDispatcher *dispatcher,
      Metrics *metrics,
      Manager *manager,
      const std::string &link_name,
      int interface_index);

  virtual ~VPN();

  virtual bool TechnologyIs(const Technology::Identifier type) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(VPN);
};

}  // namespace shill

#endif  // SHILL_VPN_
