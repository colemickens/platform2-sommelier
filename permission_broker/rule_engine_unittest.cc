// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/rule_engine.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

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

class MockRuleEngine : public RuleEngine {
 public:
  MockRuleEngine() : RuleEngine(0) {}
  ~MockRuleEngine() override = default;

  MOCK_METHOD1(MockGrantAccess, bool(const string &path));
  MOCK_METHOD0(WaitForEmptyUdevQueue, void(void));

 private:
  virtual bool GrantAccess(const string &path) {
    return MockGrantAccess(path);
  }

  DISALLOW_COPY_AND_ASSIGN(MockRuleEngine);
};

class RuleEngineTest : public testing::Test {
 public:
  RuleEngineTest() = default;
  ~RuleEngineTest() override = default;

  bool ProcessPath(const string &path, int interface_id) {
    return engine_.ProcessPath(path, interface_id);
  }

 protected:
  Rule *CreateMockRule(const Rule::Result result) const {
    MockRule *rule = new MockRule();
    EXPECT_CALL(*rule, Process(_, _))
        .WillOnce(Return(result));
    return rule;
  }

  MockRuleEngine engine_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RuleEngineTest);
};

TEST_F(RuleEngineTest, EmptyRuleChain) {
  EXPECT_CALL(engine_, MockGrantAccess(_))
      .Times(0);
  ASSERT_FALSE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

TEST_F(RuleEngineTest, AllowAccess) {
  EXPECT_CALL(engine_, MockGrantAccess("/dev/foo"))
      .WillOnce(Return(true));
  engine_.AddRule(CreateMockRule(Rule::ALLOW));
  ASSERT_TRUE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

TEST_F(RuleEngineTest, DenyAccess) {
  EXPECT_CALL(engine_, MockGrantAccess(_))
      .Times(0);
  engine_.AddRule(CreateMockRule(Rule::DENY));
  ASSERT_FALSE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

TEST_F(RuleEngineTest, DenyPrecedence) {
  EXPECT_CALL(engine_, MockGrantAccess(_))
      .Times(0);
  engine_.AddRule(CreateMockRule(Rule::ALLOW));
  engine_.AddRule(CreateMockRule(Rule::IGNORE));
  engine_.AddRule(CreateMockRule(Rule::DENY));
  ASSERT_FALSE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

TEST_F(RuleEngineTest, AllowPrecedence) {
  EXPECT_CALL(engine_, MockGrantAccess(_))
      .WillOnce(Return(true));
  engine_.AddRule(CreateMockRule(Rule::IGNORE));
  engine_.AddRule(CreateMockRule(Rule::ALLOW));
  engine_.AddRule(CreateMockRule(Rule::IGNORE));
  ASSERT_TRUE(ProcessPath("/dev/foo", Rule::ANY_INTERFACE));
}

}  // namespace permission_broker
