// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_LOGIN_METRICS_H_
#define LOGIN_MANAGER_MOCK_LOGIN_METRICS_H_

#include "login_manager/login_metrics.h"

#include <base/basictypes.h>
#include <gmock/gmock.h>

namespace login_manager {
class OwnerKey;

class MockMetrics : public LoginMetrics {
 public:
  MockMetrics();
  virtual ~MockMetrics();

  MOCK_METHOD3(SendLoginUserType, void(bool, bool, bool));
  MOCK_METHOD1(SendPolicyFilesStatus, bool(const PolicyFilesStatus&));
  MOCK_METHOD1(RecordStats, void(const char*));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockMetrics);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_LOGIN_METRICS_H_
