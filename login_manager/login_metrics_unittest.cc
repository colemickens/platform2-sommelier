// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/login_metrics.h"

#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_temp_dir.h>
#include <base/file_util.h>
#include <gtest/gtest.h>

namespace login_manager {

class LoginMetricsTest : public testing::Test {
 public:
  LoginMetricsTest() {}
  virtual ~LoginMetricsTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    metrics_.reset(new LoginMetrics(tmpdir_.path()));
  }

  int PolicyFilesStatusCode(LoginMetrics::PolicyFilesStatus status) {
    return LoginMetrics::PolicyFilesStatusCode(status);
  }

 protected:
  ScopedTempDir tmpdir_;
  scoped_ptr<LoginMetrics> metrics_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginMetricsTest);
};

TEST_F(LoginMetricsTest, AllGood) {
  LoginMetrics::PolicyFilesStatus status;
  status.owner_key_file_state = LoginMetrics::GOOD;
  status.policy_file_state = LoginMetrics::GOOD;
  status.defunct_prefs_file_state = LoginMetrics::GOOD;
  EXPECT_EQ(PolicyFilesStatusCode(status), 0 /* 000 in base-4 */);
}

TEST_F(LoginMetricsTest, AllNotThere) {
  LoginMetrics::PolicyFilesStatus status;
  EXPECT_EQ(PolicyFilesStatusCode(status), 42 /* 222 in base-4 */);
}

TEST_F(LoginMetricsTest, Bug24361) {
  LoginMetrics::PolicyFilesStatus status;
  status.owner_key_file_state = LoginMetrics::GOOD;
  status.policy_file_state = LoginMetrics::NOT_PRESENT;
  status.defunct_prefs_file_state = LoginMetrics::GOOD;
  EXPECT_EQ(PolicyFilesStatusCode(status), 8 /* 020 in base-4 */);
}

TEST_F(LoginMetricsTest, NoPrefs) {
  LoginMetrics::PolicyFilesStatus status;
  status.owner_key_file_state = LoginMetrics::GOOD;
  status.policy_file_state = LoginMetrics::GOOD;
  status.defunct_prefs_file_state = LoginMetrics::NOT_PRESENT;
  EXPECT_EQ(PolicyFilesStatusCode(status), 2 /* 002 in base-4 */);
}

TEST_F(LoginMetricsTest, SendStatus) {
  LoginMetrics::PolicyFilesStatus status;
  EXPECT_TRUE(metrics_->SendPolicyFilesStatus(status));
  EXPECT_FALSE(metrics_->SendPolicyFilesStatus(status));
}

}  // namespace login_manager
