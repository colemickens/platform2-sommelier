// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_RESOLVER_H_
#define SHILL_MOCK_RESOLVER_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/resolver.h"

namespace shill {

class MockResolver : public Resolver {
 public:
  MockResolver();
  virtual ~MockResolver();

  MOCK_METHOD1(SetDNSFromIPConfig, bool(const IPConfigRefPtr &ipconfig));
  MOCK_METHOD2(SetDNSFromLists,
               bool(const std::vector<std::string> &dns_servers,
                    const std::vector<std::string> &domain_search));
  MOCK_METHOD0(ClearDNS, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockResolver);
};

}  // namespace shill

#endif  // SHILL_MOCK_RESOLVER_H_
