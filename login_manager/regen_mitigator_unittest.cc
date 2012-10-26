// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/regen_mitigator.h"

#include <sys/types.h>
#include <unistd.h>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "chromeos/dbus/service_constants.h"
#include "login_manager/child_job.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_policy_key.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/session_manager_service.h"

using ::testing::Return;

namespace login_manager {

class RegenMitigatorTest : public ::testing::Test {
 public:
  RegenMitigatorTest() : manager_(NULL) {}
  virtual ~RegenMitigatorTest() {}

  virtual void SetUp() {
    scoped_ptr<ChildJobInterface> job(new MockChildJob());
    manager_ = new SessionManagerService(job.Pass(), 3, false, &utils_);
  }

  virtual void TearDown() {
    manager_ = NULL;
  }

 protected:
  MockSystemUtils utils_;
  scoped_refptr<SessionManagerService> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegenMitigatorTest);
};

TEST_F(RegenMitigatorTest, Mitigate) {
  MockKeyGenerator gen;
  MockPolicyKey key;
  EXPECT_CALL(gen, Start(getuid(), manager_.get()))
      .WillOnce(Return(true));
  RegenMitigator mitigator(&gen, true, getuid(), manager_.get());
  EXPECT_TRUE(mitigator.Mitigate(&key));
}

}  // namespace login_manager
