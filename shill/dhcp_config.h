// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_CONFIG_
#define SHILL_DHCP_CONFIG_

#include "shill/ipconfig.h"

namespace shill {

class DHCPConfig : public IPConfig {
 public:
  explicit DHCPConfig(const Device &device);
  virtual ~DHCPConfig();

 private:
  DISALLOW_COPY_AND_ASSIGN(DHCPConfig);
};

}  // namespace shill

#endif  // SHILL_DHCP_CONFIG_
