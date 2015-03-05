// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIREWALLD_MOCK_IPTABLES_H_
#define FIREWALLD_MOCK_IPTABLES_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "firewalld/iptables.h"

namespace firewalld {

class MockIpTables : public IpTables {
 public:
  MockIpTables();
  ~MockIpTables() override;

  MOCK_METHOD2(ApplyMasquerade, bool(const std::string& interface,
                                     bool add));

  MOCK_METHOD2(ApplyMarkForUserTraffic, bool(const std::string& user_name,
                                             bool add));

  MOCK_METHOD1(ApplyRuleForUserTraffic, bool(bool add));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIpTables);
};

}  // namespace firewalld

#endif  // FIREWALLD_MOCK_IPTABLES_H_
