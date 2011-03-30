// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy.h"

#include <base/file_util.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "login_manager/bindings/device_management_backend.pb.h"

namespace login_manager {

class DevicePolicyTest : public ::testing::Test {
 public:
  DevicePolicyTest() {}

  virtual ~DevicePolicyTest() {}

  bool StartFresh() {
    return file_util::Delete(tmpfile_, false);
  }

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());

    // Create a temporary filename that's guaranteed to not exist, but is
    // inside our scoped directory so it'll get deleted later.
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &tmpfile_));
    ASSERT_TRUE(StartFresh());

    // Dump some test data into the file.
    store_.reset(new DevicePolicy(tmpfile_));
    ASSERT_TRUE(store_->LoadOrCreate());

    policy_.set_error_message(kDefaultPolicy);
    store_->Set(policy_);

    ASSERT_TRUE(store_->Persist());
  }

  virtual void TearDown() {
  }

  void CheckExpectedPolicy(DevicePolicy* store) {
    std::string serialized;
    ASSERT_TRUE(policy_.SerializeToString(&serialized));
    std::string serialized_from;
    ASSERT_TRUE(store->Get(&serialized_from));
    EXPECT_EQ(serialized, serialized_from);
  }

  static const char kDefaultPolicy[];

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;
  scoped_ptr<DevicePolicy> store_;
  enterprise_management::PolicyFetchResponse policy_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyTest);
};

// static
const char DevicePolicyTest::kDefaultPolicy[] = "the policy";

TEST_F(DevicePolicyTest, CreateEmptyStore) {
  StartFresh();
  DevicePolicy store(tmpfile_);
  ASSERT_TRUE(store.LoadOrCreate());  // Should create an empty DictionaryValue.
  std::string serialized;
  EXPECT_TRUE(store.Get(&serialized));
  EXPECT_TRUE(serialized.empty());
}

TEST_F(DevicePolicyTest, FailBrokenStore) {
  FilePath bad_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &bad_file));
  DevicePolicy store(bad_file);
  ASSERT_FALSE(store.LoadOrCreate());
}

TEST_F(DevicePolicyTest, VerifyPolicyStorage) {
  CheckExpectedPolicy(store_.get());
}

TEST_F(DevicePolicyTest, VerifyPolicyUpdate) {
  CheckExpectedPolicy(store_.get());

  enterprise_management::PolicyFetchResponse new_policy;
  new_policy.set_error_message("new policy");
  store_->Set(new_policy);

  std::string new_out;
  ASSERT_TRUE(store_->Get(&new_out));
  std::string new_value;
  ASSERT_TRUE(new_policy.SerializeToString(&new_value));
  EXPECT_EQ(new_value, new_out);
}

TEST_F(DevicePolicyTest, LoadStoreFromDisk) {
  DevicePolicy store2(tmpfile_);
  ASSERT_TRUE(store2.LoadOrCreate());
  CheckExpectedPolicy(&store2);
}

}  // namespace login_manager
