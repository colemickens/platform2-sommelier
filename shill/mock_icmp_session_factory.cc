// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_icmp_session_factory.h"

namespace shill {

MockIcmpSessionFactory::MockIcmpSessionFactory() {}
MockIcmpSessionFactory::~MockIcmpSessionFactory() {}

MockIcmpSessionFactory* MockIcmpSessionFactory::GetInstance() {
  static base::NoDestructor<MockIcmpSessionFactory> instance;
  testing::Mock::AllowLeak(instance.get());
  return instance.get();
}

}  // namespace shill
