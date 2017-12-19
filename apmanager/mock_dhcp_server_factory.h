// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_DHCP_SERVER_FACTORY_H_
#define APMANAGER_MOCK_DHCP_SERVER_FACTORY_H_

#include <string>

#include <base/lazy_instance.h>
#include <gmock/gmock.h>

#include "apmanager/dhcp_server_factory.h"

namespace apmanager {

class MockDHCPServerFactory : public DHCPServerFactory {
 public:
  ~MockDHCPServerFactory() override;

  // This is a singleton. Use MockDHCPServerFactory::GetInstance()->Foo().
  static MockDHCPServerFactory* GetInstance();

  MOCK_METHOD2(CreateDHCPServer,
               DHCPServer*(uint16_t server_address_index,
                           const std::string& interface_name));

 protected:
  MockDHCPServerFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockDHCPServerFactory>;

  DISALLOW_COPY_AND_ASSIGN(MockDHCPServerFactory);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_DHCP_SERVER_FACTORY_H_
