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
  virtual ~MockRule() {}

  MOCK_METHOD1(Process, Result(const string &path));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRule);
};

class MockPermissionBroker : public PermissionBroker {
 public:
  MockPermissionBroker() {}
  virtual ~MockPermissionBroker() {}

  MOCK_METHOD1(MockGrantAccess, bool(const string &path));

 private:
  virtual bool GrantAccess(const string &path) {
    return MockGrantAccess(path);
  }

  DISALLOW_COPY_AND_ASSIGN(MockPermissionBroker);
};

class PermissionBrokerTest : public testing::Test {
 public:
  PermissionBrokerTest() {}
  virtual ~PermissionBrokerTest() {}

  bool ProcessPath(const string &path) {
    return broker_.ProcessPath(path);
  }

 protected:
  Rule *CreateMockRule(const Rule::Result result) const {
    MockRule *rule = new MockRule();
    EXPECT_CALL(*rule, Process(_))
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
  ASSERT_FALSE(ProcessPath("/dev/foo"));
}

TEST_F(PermissionBrokerTest, AllowAccess) {
  EXPECT_CALL(broker_, MockGrantAccess("/dev/foo"))
      .WillOnce(Return(true));
  broker_.AddRule(CreateMockRule(Rule::ALLOW));
  ASSERT_TRUE(ProcessPath("/dev/foo"));
}

TEST_F(PermissionBrokerTest, DenyAccess) {
  EXPECT_CALL(broker_, MockGrantAccess(_))
      .Times(0);
  broker_.AddRule(CreateMockRule(Rule::DENY));
  ASSERT_FALSE(ProcessPath("/dev/foo"));
}

TEST_F(PermissionBrokerTest, DenyPrecedence) {
  EXPECT_CALL(broker_, MockGrantAccess(_))
      .Times(0);
  broker_.AddRule(CreateMockRule(Rule::ALLOW));
  broker_.AddRule(CreateMockRule(Rule::IGNORE));
  broker_.AddRule(CreateMockRule(Rule::DENY));
  ASSERT_FALSE(ProcessPath("/dev/foo"));
}

TEST_F(PermissionBrokerTest, AllowPrecedence) {
  EXPECT_CALL(broker_, MockGrantAccess(_))
      .WillOnce(Return(true));
  broker_.AddRule(CreateMockRule(Rule::IGNORE));
  broker_.AddRule(CreateMockRule(Rule::ALLOW));
  broker_.AddRule(CreateMockRule(Rule::IGNORE));
  ASSERT_TRUE(ProcessPath("/dev/foo"));
}

}  // namespace permission_broker
