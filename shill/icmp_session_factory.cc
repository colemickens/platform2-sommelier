// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/icmp_session_factory.h"

namespace shill {

namespace {
base::LazyInstance<IcmpSessionFactory> g_icmp_session_factory =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

IcmpSessionFactory::IcmpSessionFactory() {}
IcmpSessionFactory::~IcmpSessionFactory() {}

IcmpSessionFactory* IcmpSessionFactory::GetInstance() {
  return g_icmp_session_factory.Pointer();
}

IcmpSession* IcmpSessionFactory::CreateIcmpSession(
    EventDispatcher* dispatcher) {
  return new IcmpSession(dispatcher);
}
// found in the LICENSE file.

}  // namespace shill
