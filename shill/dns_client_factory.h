// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DNS_CLIENT_FACTORY_H_
#define SHILL_DNS_CLIENT_FACTORY_H_

#include <string>
#include <vector>

#include <base/lazy_instance.h>

#include "shill/dns_client.h"
#include "shill/event_dispatcher.h"
#include "shill/net/ip_address.h"

namespace shill {

class DNSClientFactory {
 public:
  virtual ~DNSClientFactory();

  // This is a singleton. Use DNSClientFactory::GetInstance()->Foo().
  static DNSClientFactory* GetInstance();

  virtual DNSClient* CreateDNSClient(
      IPAddress::Family family,
      const std::string& interface_name,
      const std::vector<std::string>& dns_servers,
      int timeout_ms,
      EventDispatcher* dispatcher,
      const DNSClient::ClientCallback& callback);

 protected:
  DNSClientFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<DNSClientFactory>;

  DISALLOW_COPY_AND_ASSIGN(DNSClientFactory);
};

}  // namespace shill

#endif  // SHILL_DNS_CLIENT_FACTORY_H_
