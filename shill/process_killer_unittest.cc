// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/process_killer.h"

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using base::Bind;
using base::Closure;
using base::Unretained;

namespace shill {

class ProcessKillerTest : public testing::Test {
 public:
  ProcessKillerTest() : process_killer_(ProcessKiller::GetInstance()) {}

 protected:
  class Target {
   public:
    virtual ~Target() {}

    MOCK_METHOD0(Call, void());
  };

  ProcessKiller* process_killer_;
};

TEST_F(ProcessKillerTest, OnProcessDied) {
  const int kPID = 123;
  EXPECT_TRUE(process_killer_->callbacks_.empty());
  // Expect no crash.
  ProcessKiller::OnProcessDied(kPID, 0, process_killer_);
  process_killer_->callbacks_[kPID] = Closure();
  // Expect no crash.
  ProcessKiller::OnProcessDied(kPID, 0, process_killer_);
  EXPECT_TRUE(process_killer_->callbacks_.empty());
  Target target;
  EXPECT_CALL(target, Call());
  process_killer_->callbacks_[kPID] = Bind(&Target::Call, Unretained(&target));
  ProcessKiller::OnProcessDied(kPID, 0, process_killer_);
  EXPECT_TRUE(process_killer_->callbacks_.empty());
}

}  // namespace shill
