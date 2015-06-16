// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_dns_client_factory.h"

namespace shill {

namespace {
base::LazyInstance<MockDNSClientFactory> g_mock_dns_client_factory
    = LAZY_INSTANCE_INITIALIZER;
}  // namespace

MockDNSClientFactory::MockDNSClientFactory() {}
MockDNSClientFactory::~MockDNSClientFactory() {}

MockDNSClientFactory* MockDNSClientFactory::GetInstance() {
  return g_mock_dns_client_factory.Pointer();
}

}  // namespace shill
