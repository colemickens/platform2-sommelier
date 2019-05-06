// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/icmp_session_factory.h"

#include <memory>

namespace shill {

IcmpSessionFactory::IcmpSessionFactory() = default;
IcmpSessionFactory::~IcmpSessionFactory() = default;

IcmpSessionFactory* IcmpSessionFactory::GetInstance() {
  static base::NoDestructor<IcmpSessionFactory> instance;
  return instance.get();
}

std::unique_ptr<IcmpSession> IcmpSessionFactory::CreateIcmpSession(
    EventDispatcher* dispatcher) {
  return std::make_unique<IcmpSession>(dispatcher);
}

}  // namespace shill
