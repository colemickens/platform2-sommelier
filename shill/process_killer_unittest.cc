//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
