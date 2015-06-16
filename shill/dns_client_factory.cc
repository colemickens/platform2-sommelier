// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dns_client_factory.h"

using std::string;
using std::vector;

namespace shill {

namespace {

base::LazyInstance<DNSClientFactory> g_dns_client_factory
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

DNSClientFactory::DNSClientFactory() {}
DNSClientFactory::~DNSClientFactory() {}

DNSClientFactory* DNSClientFactory::GetInstance() {
  return g_dns_client_factory.Pointer();
}

DNSClient* DNSClientFactory::CreateDNSClient(
    IPAddress::Family family,
    const string& interface_name,
    const vector<string>& dns_servers,
    int timeout_ms,
    EventDispatcher* dispatcher,
    const DNSClient::ClientCallback& callback) {
  return new DNSClient(family,
                       interface_name,
                       dns_servers,
                       timeout_ms,
                       dispatcher,
                       callback);
}

}  // namespace shill
