// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MOCK_SIM_PROXY_
#define SHILL_MM1_MOCK_SIM_PROXY_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/mm1_sim_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockSimProxy : public SimProxyInterface {
 public:
  MockSimProxy();
  virtual ~MockSimProxy();

  MOCK_METHOD4(SendPin, void(const std::string &pin,
                             Error *error,
                             const ResultCallback &callback,
                             int timeout));
  MOCK_METHOD5(SendPuk, void(const std::string &puk,
                             const std::string &pin,
                             Error *error,
                             const ResultCallback &callback,
                             int timeout));
  MOCK_METHOD5(EnablePin, void(const std::string &pin,
                               const bool enabled,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout));
  MOCK_METHOD5(ChangePin, void(const std::string &old_pin,
                               const std::string &new_pin,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout));

  MOCK_METHOD0(SimIdentifier, const std::string());
  MOCK_METHOD0(Imsi, const std::string());
  MOCK_METHOD0(OperatorIdentifier, const std::string());
  MOCK_METHOD0(OperatorName, const std::string());

  DISALLOW_COPY_AND_ASSIGN(MockSimProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MOCK_SIM_PROXY_
