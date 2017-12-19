// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/mock_dhcp_server_factory.h"

namespace apmanager {

namespace {
base::LazyInstance<MockDHCPServerFactory> g_mock_dhcp_server_factory
    = LAZY_INSTANCE_INITIALIZER;
}  // namespace

MockDHCPServerFactory::MockDHCPServerFactory() {}
MockDHCPServerFactory::~MockDHCPServerFactory() {}

MockDHCPServerFactory* MockDHCPServerFactory::GetInstance() {
  return g_mock_dhcp_server_factory.Pointer();
}

}  // namespace apmanager
