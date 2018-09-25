// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CONNECTIVITY_TRIAL_H_
#define SHILL_MOCK_CONNECTIVITY_TRIAL_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/connectivity_trial.h"

namespace shill {

class MockConnectivityTrial : public ConnectivityTrial {
 public:
  MockConnectivityTrial(ConnectionRefPtr connection, int trial_timeout_seconds);
  ~MockConnectivityTrial() override;

  MOCK_METHOD2(Start, bool(const std::string&, int));
  MOCK_METHOD1(Retry, bool(int));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(IsActive, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectivityTrial);
};

}  // namespace shill

#endif  // SHILL_MOCK_CONNECTIVITY_TRIAL_H_
