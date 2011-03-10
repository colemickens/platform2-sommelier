// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy.h"

#include <base/file_util.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/scoped_temp_dir.h>
#include <gtest/gtest.h>

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
    ASSERT_TRUE(store_->Load());

    store_->Set(kDefaultPolicy, kDefaultPolicySig);

    ASSERT_TRUE(store_->Persist());
  }

  virtual void TearDown() {
  }

  void CheckExpectedPolicy(DevicePolicy* store) {
    std::string tmp_value;
    std::string tmp_sig;

    EXPECT_TRUE(store->Get(&tmp_value, &tmp_sig));
    EXPECT_EQ(kDefaultPolicy, tmp_value);
    EXPECT_EQ(kDefaultPolicySig, tmp_sig);
  }

  static const char kDefaultPolicy[];
  static const char kDefaultPolicySig[];

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;
  scoped_ptr<DevicePolicy> store_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyTest);
};

// static
const char DevicePolicyTest::kDefaultPolicy[] = "the policy";
// static
const char DevicePolicyTest::kDefaultPolicySig[] = "the policy signature";

TEST_F(DevicePolicyTest, CreateEmptyStore) {
  StartFresh();
  DevicePolicy store(tmpfile_);
  ASSERT_TRUE(store.Load());  // Should create an empty DictionaryValue.
  std::string foo, bar;
  EXPECT_FALSE(store.Get(&foo, &bar));
  EXPECT_TRUE(foo.empty());
  EXPECT_TRUE(bar.empty());
}

TEST_F(DevicePolicyTest, FailBrokenStore) {
  FilePath bad_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &bad_file));
  DevicePolicy store(bad_file);
  ASSERT_FALSE(store.Load());
}

TEST_F(DevicePolicyTest, VerifyPolicyStorage) {
  CheckExpectedPolicy(store_.get());
}

TEST_F(DevicePolicyTest, VerifyPolicyUpdate) {
  CheckExpectedPolicy(store_.get());

  std::string tmp_value;
  std::string tmp_sig;
  std::string new_value("new policy");
  std::string new_sig("new policy sig");
  store_->Set(new_value, new_sig);

  EXPECT_TRUE(store_->Get(&tmp_value, &tmp_sig));
  EXPECT_EQ(new_value, tmp_value);
  EXPECT_EQ(new_sig, tmp_sig);
}

TEST_F(DevicePolicyTest, LoadStoreFromDisk) {
  DevicePolicy store2(tmpfile_);
  ASSERT_TRUE(store2.Load());
  CheckExpectedPolicy(&store2);
}

}  // namespace login_manager
