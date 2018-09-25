// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_icmp_session_factory.h"

namespace shill {

namespace {
base::LazyInstance<MockIcmpSessionFactory>::Leaky g_mock_icmp_session_factory =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

MockIcmpSessionFactory::MockIcmpSessionFactory() {}
MockIcmpSessionFactory::~MockIcmpSessionFactory() {}

MockIcmpSessionFactory* MockIcmpSessionFactory::GetInstance() {
  MockIcmpSessionFactory* instance = g_mock_icmp_session_factory.Pointer();
  testing::Mock::AllowLeak(instance);
  return instance;
}

}  // namespace shill
