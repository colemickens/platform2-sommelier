// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/dhcp_server_factory.h"

namespace apmanager {

namespace {

base::LazyInstance<DHCPServerFactory> g_dhcp_server_factory
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

DHCPServerFactory::DHCPServerFactory() {}
DHCPServerFactory::~DHCPServerFactory() {}

DHCPServerFactory* DHCPServerFactory::GetInstance() {
  return g_dhcp_server_factory.Pointer();
}

DHCPServer* DHCPServerFactory::CreateDHCPServer(
    uint16_t server_addr_index, const std::string& interface_name) {
  return new DHCPServer(server_addr_index, interface_name);
}

}  // namespace apmanager
