// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DNS_CLIENT_FACTORY_H_
#define SHILL_MOCK_DNS_CLIENT_FACTORY_H_

#include <string>
#include <vector>

#include <base/lazy_instance.h>
#include <gmock/gmock.h>

#include "shill/dns_client_factory.h"
#include "shill/event_dispatcher.h"
#include "shill/net/ip_address.h"

namespace shill {

class MockDNSClientFactory : public DNSClientFactory {
 public:
  ~MockDNSClientFactory() override;

  // This is a singleton. Use MockDNSClientFactory::GetInstance()->Foo().
  static MockDNSClientFactory* GetInstance();

  MOCK_METHOD6(CreateDNSClient,
               DNSClient* (IPAddress::Family family,
                           const std::string& interface_name,
                           const std::vector<std::string>& dns_servers,
                           int timeout_ms,
                           EventDispatcher* dispatcher,
                           const DNSClient::ClientCallback& callback));

 protected:
  MockDNSClientFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockDNSClientFactory>;

  DISALLOW_COPY_AND_ASSIGN(MockDNSClientFactory);
};

}  // namespace shill

#endif  // SHILL_MOCK_DNS_CLIENT_FACTORY_H_
