//
// Copyright (C) 2013 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/dns_client_factory.h"

#include <memory>

using std::string;
using std::vector;

namespace shill {

namespace {

base::LazyInstance<DnsClientFactory>::Leaky g_dns_client_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DnsClientFactory::DnsClientFactory() {}
DnsClientFactory::~DnsClientFactory() {}

DnsClientFactory* DnsClientFactory::GetInstance() {
  return g_dns_client_factory.Pointer();
}

std::unique_ptr<DnsClient> DnsClientFactory::CreateDnsClient(
    IPAddress::Family family,
    const string& interface_name,
    const vector<string>& dns_servers,
    int timeout_ms,
    EventDispatcher* dispatcher,
    const DnsClient::ClientCallback& callback) {
  return std::make_unique<DnsClient>(family,
                                     interface_name,
                                     dns_servers,
                                     timeout_ms,
                                     dispatcher,
                                     callback);
}

}  // namespace shill
