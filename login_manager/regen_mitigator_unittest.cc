// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/regen_mitigator.h"

#include <unistd.h>
#include <sys/types.h>

#include <base/ref_counted.h>
#include <gtest/gtest.h>

#include "chromeos/dbus/service_constants.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_owner_key.h"
#include "login_manager/session_manager_service.h"

using ::testing::Return;

namespace login_manager {

class RegenMitigatorTest : public ::testing::Test {
 public:
  RegenMitigatorTest() : manager_(NULL) {}
  virtual ~RegenMitigatorTest() {}

  virtual void SetUp() {
    std::vector<ChildJobInterface*> jobs;
    manager_ = new SessionManagerService(jobs);
  }

  virtual void TearDown() {
    manager_ = NULL;
  }

 protected:
  scoped_refptr<SessionManagerService> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegenMitigatorTest);
};

TEST_F(RegenMitigatorTest, Mitigate) {
  MockKeyGenerator gen;
  MockOwnerKey key;
  EXPECT_CALL(gen, Start(getuid(), &key, manager_.get()))
      .WillOnce(Return(true));
  RegenMitigator mitigator(&gen, true, getuid(), manager_.get());
  EXPECT_TRUE(mitigator.Mitigate(&key));
}

}  // namespace login_manager
