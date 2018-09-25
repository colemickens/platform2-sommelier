// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_dns_client_factory.h"

namespace shill {

namespace {
base::LazyInstance<MockDnsClientFactory>::Leaky g_mock_dns_client_factory =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

MockDnsClientFactory::MockDnsClientFactory() {}
MockDnsClientFactory::~MockDnsClientFactory() {}

MockDnsClientFactory* MockDnsClientFactory::GetInstance() {
  MockDnsClientFactory* instance = g_mock_dns_client_factory.Pointer();
  testing::Mock::AllowLeak(instance);
  return instance;
}

}  // namespace shill
