// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_MOCK_DHCP_PROPERTIES_H_
#define SHILL_DHCP_MOCK_DHCP_PROPERTIES_H_

#include "shill/dhcp/dhcp_properties.h"

#include <string>

#include <gmock/gmock.h>

namespace shill {

class MockDhcpProperties : public DhcpProperties {
 public:
  MockDhcpProperties();
  ~MockDhcpProperties() override;

  MOCK_CONST_METHOD2(Save, void(StoreInterface* store, const std::string& id));
  MOCK_METHOD2(Load, void(StoreInterface* store, const std::string& id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDhcpProperties);
};

}  // namespace shill

#endif  // SHILL_DHCP_MOCK_DHCP_PROPERTIES_H_
