// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DHCP_SERVER_FACTORY_H_
#define APMANAGER_DHCP_SERVER_FACTORY_H_

#include <string>

#include <base/lazy_instance.h>

#include "apmanager/dhcp_server.h"

namespace apmanager {

class DHCPServerFactory {
 public:
  virtual ~DHCPServerFactory();

  // This is a singleton. Use DHCPServerFactory::GetInstance()->Foo().
  static DHCPServerFactory* GetInstance();

  virtual DHCPServer* CreateDHCPServer(uint16_t server_address_index,
                                       const std::string& interface_name);

 protected:
  DHCPServerFactory();

 private:
  friend base::LazyInstanceTraitsBase<DHCPServerFactory>;

  DISALLOW_COPY_AND_ASSIGN(DHCPServerFactory);
};

}  // namespace apmanager

#endif  // APMANAGER_DHCP_SERVER_FACTORY_H_
