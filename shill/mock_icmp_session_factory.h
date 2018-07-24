// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ICMP_SESSION_FACTORY_H_
#define SHILL_MOCK_ICMP_SESSION_FACTORY_H_

#include <base/lazy_instance.h>
#include <gmock/gmock.h>

#include "shill/icmp_session_factory.h"

namespace shill {

class MockIcmpSessionFactory : public IcmpSessionFactory {
 public:
  ~MockIcmpSessionFactory() override;

  // This is a singleton. Use MockIcmpSessionFactory::GetInstance()->Foo().
  static MockIcmpSessionFactory* GetInstance();

  MOCK_METHOD1(CreateIcmpSession, IcmpSession*(EventDispatcher* dispatcher));

 protected:
  MockIcmpSessionFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockIcmpSessionFactory>;

  DISALLOW_COPY_AND_ASSIGN(MockIcmpSessionFactory);
};

}  // namespace shill

#endif  // SHILL_MOCK_ICMP_SESSION_FACTORY_H_
