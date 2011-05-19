// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/policy_store.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace em = enterprise_management;

namespace login_manager {

class PolicyStoreTest : public ::testing::Test {
 public:
  PolicyStoreTest() {}

  virtual ~PolicyStoreTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());

    // Create a temporary filename that's guaranteed to not exist, but is
    // inside our scoped directory so it'll get deleted later.
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &tmpfile_));
    ASSERT_TRUE(file_util::Delete(tmpfile_, false));
  }

  virtual void TearDown() {
  }

  void CheckExpectedPolicy(PolicyStore* store,
                           const em::PolicyFetchResponse& policy) {
    std::string serialized;
    ASSERT_TRUE(policy.SerializeToString(&serialized));
    std::string serialized_from;
    ASSERT_TRUE(store->SerializeToString(&serialized_from));
    EXPECT_EQ(serialized, serialized_from);
  }

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyStoreTest);
};

TEST_F(PolicyStoreTest, CreateEmptyStore) {
  PolicyStore store(tmpfile_);
  ASSERT_TRUE(store.LoadOrCreate());  // Should create an empty policy.
  std::string serialized;
  EXPECT_TRUE(store.SerializeToString(&serialized));
  EXPECT_TRUE(serialized.empty());
}

TEST_F(PolicyStoreTest, FailBrokenStore) {
  FilePath bad_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &bad_file));
  PolicyStore store(bad_file);
  ASSERT_FALSE(store.LoadOrCreate());
}

TEST_F(PolicyStoreTest, VerifyPolicyStorage) {
  enterprise_management::PolicyFetchResponse policy;
  policy.set_error_message("policy");
  PolicyStore store(tmpfile_);
  store.Set(policy);
  CheckExpectedPolicy(&store, policy);
}

TEST_F(PolicyStoreTest, VerifyPolicyUpdate) {
  PolicyStore store(tmpfile_);
  enterprise_management::PolicyFetchResponse policy;
  policy.set_error_message("policy");
  store.Set(policy);
  CheckExpectedPolicy(&store, policy);

  enterprise_management::PolicyFetchResponse new_policy;
  new_policy.set_error_message("new policy");
  store.Set(new_policy);
  CheckExpectedPolicy(&store, new_policy);
}

TEST_F(PolicyStoreTest, LoadStoreFromDisk) {
  PolicyStore store(tmpfile_);
  enterprise_management::PolicyFetchResponse policy;
  policy.set_error_message("policy");
  store.Set(policy);
  ASSERT_TRUE(store.Persist());
  CheckExpectedPolicy(&store, policy);

  PolicyStore store2(tmpfile_);
  ASSERT_TRUE(store2.LoadOrCreate());
  CheckExpectedPolicy(&store2, policy);
}

}  // namespace login_manager
