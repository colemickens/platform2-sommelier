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

  MOCK_METHOD1(Process, Result(const string& path));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRule);
};

class MockRuleEngine : public RuleEngine {
 public:
  MockRuleEngine() : RuleEngine() {}
  ~MockRuleEngine() override = default;

  MOCK_METHOD0(WaitForEmptyUdevQueue, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRuleEngine);
};

class RuleEngineTest : public testing::Test {
 public:
  RuleEngineTest() = default;
  ~RuleEngineTest() override = default;

  Rule::Result ProcessPath(const string& path) {
    return engine_.ProcessPath(path);
  }

 protected:
  Rule *CreateMockRule(const Rule::Result result) const {
    MockRule *rule = new MockRule();
    EXPECT_CALL(*rule, Process(_)).WillOnce(Return(result));
    return rule;
  }

  MockRuleEngine engine_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RuleEngineTest);
};

TEST_F(RuleEngineTest, EmptyRuleChain) {
  EXPECT_EQ(Rule::IGNORE, ProcessPath("/dev/foo"));
}

TEST_F(RuleEngineTest, AllowAccess) {
  engine_.AddRule(CreateMockRule(Rule::ALLOW));
  EXPECT_EQ(Rule::ALLOW, ProcessPath("/dev/foo"));
}

TEST_F(RuleEngineTest, DenyAccess) {
  engine_.AddRule(CreateMockRule(Rule::DENY));
  EXPECT_EQ(Rule::DENY, ProcessPath("/dev/foo"));
}

TEST_F(RuleEngineTest, DenyPrecedence) {
  engine_.AddRule(CreateMockRule(Rule::ALLOW));
  engine_.AddRule(CreateMockRule(Rule::IGNORE));
  engine_.AddRule(CreateMockRule(Rule::DENY));
  EXPECT_EQ(Rule::DENY, ProcessPath("/dev/foo"));
}

TEST_F(RuleEngineTest, AllowPrecedence) {
  engine_.AddRule(CreateMockRule(Rule::IGNORE));
  engine_.AddRule(CreateMockRule(Rule::ALLOW));
  engine_.AddRule(CreateMockRule(Rule::IGNORE));
  EXPECT_EQ(Rule::ALLOW, ProcessPath("/dev/foo"));
}

TEST_F(RuleEngineTest, LockdownPrecedence) {
  engine_.AddRule(CreateMockRule(Rule::IGNORE));
  engine_.AddRule(CreateMockRule(Rule::ALLOW_WITH_LOCKDOWN));
  engine_.AddRule(CreateMockRule(Rule::ALLOW));
  EXPECT_EQ(Rule::ALLOW_WITH_LOCKDOWN, ProcessPath("/dev/foo"));
}

}  // namespace permission_broker
