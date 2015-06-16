// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CONNECTION_HEALTH_CHECKER_H_
#define SHILL_MOCK_CONNECTION_HEALTH_CHECKER_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/connection_health_checker.h"

namespace shill {

class MockConnectionHealthChecker : public ConnectionHealthChecker {
 public:
  MockConnectionHealthChecker(
      ConnectionRefPtr connection,
      EventDispatcher* dispatcher,
      IPAddressStore* remote_ips,
      const base::Callback<void(Result)>& result_callback);
  ~MockConnectionHealthChecker() override;

  MOCK_METHOD1(AddRemoteURL, void(const std::string& url_string));
  MOCK_METHOD1(AddRemoteIP, void(IPAddress ip));
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(health_check_in_progress, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionHealthChecker);
};

}  // namespace shill

#endif  // SHILL_MOCK_CONNECTION_HEALTH_CHECKER_H_
