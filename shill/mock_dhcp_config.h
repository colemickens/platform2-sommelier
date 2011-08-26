// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DHCP_CONFIG_H_
#define SHILL_MOCK_DHCP_CONFIG_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/dhcp_config.h"

namespace shill {

class MockDHCPConfig : public DHCPConfig {
 public:
  MockDHCPConfig(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 DHCPProvider *provider,
                 const std::string &device_name,
                 GLib *glib);
  virtual ~MockDHCPConfig();

  MOCK_METHOD0(RequestIP, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDHCPConfig);
};

}  // namespace shill

#endif  // SHILL_MOCK_DHCP_CONFIG_H_
