// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <base/macros.h>
#include <gtest/gtest.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>

#include "authpolicy/proto_bindings/active_directory_info.pb.h"
#include "authpolicy/windows_policy_manager.h"
#include "bindings/authpolicy_containers.pb.h"

namespace authpolicy {

using WindowsPolicy = protos::WindowsPolicy;

class WindowsPolicyManagerTest : public ::testing::Test {
 public:
  WindowsPolicyManagerTest() = default;

  void SetUp() override {
    base::FilePath tmp_path;
    CHECK(base::CreateNewTempDirectory("" /* prefix (ignored) */, &tmp_path));

    policy_path_ = tmp_path.Append("windows_policy");
    manager_ = std::make_unique<WindowsPolicyManager>(policy_path_);
  }

 protected:
  void ExpectUpdateAndSaveSucceeds(const WindowsPolicy& policy) {
    auto policy_unique_ptr = std::make_unique<WindowsPolicy>(policy);
    const WindowsPolicy* policy_ptr = policy_unique_ptr.get();
    EXPECT_EQ(ERROR_NONE,
              manager_->UpdateAndSaveToDisk(std::move(policy_unique_ptr)));
    EXPECT_EQ(policy_ptr, manager_->policy());
    EXPECT_TRUE(base::PathExists(policy_path_));
  }

  base::FilePath policy_path_;
  std::unique_ptr<WindowsPolicyManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowsPolicyManagerTest);
};

// Tests updating and reloading policy.
TEST_F(WindowsPolicyManagerTest, UpdateSaveAndLoadSucceeds) {
  EXPECT_FALSE(base::PathExists(policy_path_));
  WindowsPolicy policy;
  policy.set_user_policy_mode(WindowsPolicy::USER_POLICY_MODE_REPLACE);
  ExpectUpdateAndSaveSucceeds(policy);

  // Write another time (makes sure file permissions don't prevent this).
  policy.set_user_policy_mode(WindowsPolicy::USER_POLICY_MODE_MERGE);
  ExpectUpdateAndSaveSucceeds(policy);

  manager_ = std::make_unique<WindowsPolicyManager>(policy_path_);
  EXPECT_EQ(nullptr, manager_->policy());
  EXPECT_EQ(ERROR_NONE, manager_->LoadFromDisk());
  EXPECT_NE(nullptr, manager_->policy());
  EXPECT_EQ(policy.SerializeAsString(),
            manager_->policy()->SerializeAsString());
}

// Loading an empty policy works just fine (usual condition new installs).
TEST_F(WindowsPolicyManagerTest, LoadWithMissingFileSucceeds) {
  EXPECT_EQ(ERROR_NONE, manager_->LoadFromDisk());
  EXPECT_EQ(nullptr, manager_->policy());
}

}  // namespace authpolicy
