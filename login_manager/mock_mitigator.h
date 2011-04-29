// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_MITIGATOR_H_
#define LOGIN_MANAGER_MOCK_MITIGATOR_H_

#include "base/basictypes.h"
#include "login_manager/owner_key_loss_mitigator.h"

#include <gmock/gmock.h>

namespace login_manager {
class OwnerKey;

class MockMitigator : public OwnerKeyLossMitigator {
 public:
  MockMitigator() {}
  virtual ~MockMitigator() {}

  MOCK_METHOD1(Mitigate, bool(OwnerKey*));
  MOCK_METHOD0(Mitigating, bool());
 private:
  DISALLOW_COPY_AND_ASSIGN(MockMitigator);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_MITIGATOR_H_
