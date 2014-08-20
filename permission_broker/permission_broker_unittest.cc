// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "base/memory/scoped_ptr.h"
#include "permission_broker/permission_broker.h"
#include "permission_broker/rule.h"

using std::string;
using ::testing::_;
using ::testing::Return;

namespace permission_broker {

class MockRule : public Rule {
 public:
  MockRule() : Rule("MockRule") {}
  ~MockRule() override = default;

  MOCK_METHOD2(Process, Result(const string &path, int interface_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRule);
};

class MockPermissionBroker : public PermissionBroker {
 public:
  MockPermissionBroker() : PermissionBroker(0) {}
  ~MockPermissionBroker() override = default;

  MOCK_METHOD1(MockGrantAccess, bool(const string &path));
  MOCK_METHOD0(WaitForEmptyUdevQueue, void(void));

 private:
  virtual bool GrantAccess(const string &path) {
    return MockGrantAccess(path);
  }

  DISALLOW_COPY_AND_ASSIGN(MockPermissionBroker);
};

class PermissionBrokerTest : public testing::Test {
 public:
  PermissionBrokerTest() = default;
  ~PermissionBrokerTest() override = default;

  bool ProcessPath(const string &path, int interface_id) {
    return broker_.ProcessPath(path, interface_id);
  }

 protected:
  Rule *CreateMockRule(const Rule::Result result) const {
    MockRule *rule = new MockRule();
    EXPECT_CALL(*rule, Process(_, _))
        .WillOnce(Return(result));
    return rule;
  }

  MockPermissionBroker broker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionBrokerTest);
};

TEST_F(PermissionBrokerTest, EmptyRuleChain) {
  EXPECT_CALL(broker_, MockGrantAccess(_))
      .Times(0);
  ASSERT_FALSE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

TEST_F(PermissionBrokerTest, AllowAccess) {
  EXPECT_CALL(broker_, MockGrantAccess("/dev/foo"))
      .WillOnce(Return(true));
  broker_.AddRule(CreateMockRule(Rule::ALLOW));
  ASSERT_TRUE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

TEST_F(PermissionBrokerTest, DenyAccess) {
  EXPECT_CALL(broker_, MockGrantAccess(_))
      .Times(0);
  broker_.AddRule(CreateMockRule(Rule::DENY));
  ASSERT_FALSE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

TEST_F(PermissionBrokerTest, DenyPrecedence) {
  EXPECT_CALL(broker_, MockGrantAccess(_))
      .Times(0);
  broker_.AddRule(CreateMockRule(Rule::ALLOW));
  broker_.AddRule(CreateMockRule(Rule::IGNORE));
  broker_.AddRule(CreateMockRule(Rule::DENY));
  ASSERT_FALSE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

TEST_F(PermissionBrokerTest, AllowPrecedence) {
  EXPECT_CALL(broker_, MockGrantAccess(_))
      .WillOnce(Return(true));
  broker_.AddRule(CreateMockRule(Rule::IGNORE));
  broker_.AddRule(CreateMockRule(Rule::ALLOW));
  broker_.AddRule(CreateMockRule(Rule::IGNORE));
  ASSERT_TRUE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

}  // namespace permission_broker
