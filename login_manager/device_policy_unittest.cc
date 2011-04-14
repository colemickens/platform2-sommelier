// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy.h"

#include <base/file_util.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "login_manager/bindings/chrome_device_policy.pb.h"
#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/mock_owner_key.h"

namespace em = enterprise_management;

namespace login_manager {
using google::protobuf::RepeatedPtrField;
using std::string;
using ::testing::Return;
using ::testing::_;

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
    ASSERT_TRUE(store->SerializeToString(&serialized_from));
    EXPECT_EQ(serialized, serialized_from);
  }

  void ExtractPolicyValue(const DevicePolicy& pol,
                          em::ChromeDeviceSettingsProto* polval) {
    em::PolicyData poldata;
    ASSERT_TRUE(pol.Get().has_policy_data());
    ASSERT_TRUE(poldata.ParseFromString(pol.Get().policy_data()));
    ASSERT_TRUE(poldata.has_policy_type());
    ASSERT_EQ(poldata.policy_type(), DevicePolicy::kDevicePolicyType);
    ASSERT_TRUE(poldata.has_policy_value());
    ASSERT_TRUE(polval->ParseFromString(poldata.policy_value()));
  }

  int CountOwnerInWhitelist(const DevicePolicy& pol, const std::string& owner) {
    em::ChromeDeviceSettingsProto polval;
    ExtractPolicyValue(pol, &polval);
    const em::UserWhitelistProto& whitelist_proto = polval.user_whitelist();
    int whitelist_count = 0;
    const RepeatedPtrField<std::string>& whitelist =
        whitelist_proto.user_whitelist();
    for (RepeatedPtrField<std::string>::const_iterator it = whitelist.begin();
         it != whitelist.end();
         ++it) {
      whitelist_count += (owner == *it ? 1 : 0);
    }
    return whitelist_count;
  }

  bool AreNewUsersAllowed(const DevicePolicy& pol) {
    em::ChromeDeviceSettingsProto polval;
    ExtractPolicyValue(pol, &polval);
    return (polval.has_allow_new_users() &&
            polval.allow_new_users().has_allow_new_users() &&
            polval.allow_new_users().allow_new_users());
  }

  em::PolicyFetchResponse Wrap(const em::ChromeDeviceSettingsProto& polval,
                               const std::string& user) {
    em::PolicyData new_poldata;
    new_poldata.set_policy_type(DevicePolicy::kDevicePolicyType);
    new_poldata.set_policy_value(polval.SerializeAsString());
    if (!user.empty())
      new_poldata.set_username(user);
    em::PolicyFetchResponse new_policy;
    new_policy.set_policy_data(new_poldata.SerializeAsString());
    return new_policy;
  }

  em::PolicyFetchResponse CreateWithOwner(const std::string& owner) {
    em::ChromeDeviceSettingsProto new_polval;
    new_polval.mutable_user_whitelist()->add_user_whitelist(owner);
    new_polval.mutable_allow_new_users()->set_allow_new_users(true);
    return Wrap(new_polval, owner);
  }

  em::PolicyFetchResponse CreateWithWhitelist(
      const std::vector<std::string>& users) {
    em::ChromeDeviceSettingsProto polval;
    polval.mutable_allow_new_users()->set_allow_new_users(true);
    em::UserWhitelistProto* whitelist_proto = polval.mutable_user_whitelist();
    for(std::vector<std::string>::const_iterator it = users.begin();
        it != users.end();
        ++it) {
      whitelist_proto->add_user_whitelist(*it);
    }
    return Wrap(polval, std::string());
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
  ASSERT_TRUE(store.LoadOrCreate());  // Should create an empty policy.
  std::string serialized;
  EXPECT_TRUE(store.SerializeToString(&serialized));
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
  ASSERT_TRUE(store_->SerializeToString(&new_out));
  std::string new_value;
  ASSERT_TRUE(new_policy.SerializeToString(&new_value));
  EXPECT_EQ(new_value, new_out);
}

TEST_F(DevicePolicyTest, LoadStoreFromDisk) {
  DevicePolicy store2(tmpfile_);
  ASSERT_TRUE(store2.LoadOrCreate());
  CheckExpectedPolicy(&store2);
}

TEST_F(DevicePolicyTest, FreshPolicy) {
  StartFresh();
  DevicePolicy pol(tmpfile_);
  ASSERT_TRUE(pol.LoadOrCreate());  // Should create an empty policy.

  std::string current_user("me");
  scoped_ptr<MockOwnerKey> key(new MockOwnerKey);
  EXPECT_CALL(*key.get(), Sign(_, _, _))
      .WillOnce(Return(true));
  pol.StoreOwnerProperties(key.get(), current_user, NULL);

  ASSERT_EQ(CountOwnerInWhitelist(pol, current_user), 1);
}

TEST_F(DevicePolicyTest, OwnerAlreadyInPolicy) {
  StartFresh();
  DevicePolicy pol(tmpfile_);
  ASSERT_TRUE(pol.LoadOrCreate());  // Should create an empty policy.

  std::string current_user("me");
  pol.Set(CreateWithOwner(current_user));

  scoped_ptr<MockOwnerKey> key(new MockOwnerKey);
  EXPECT_CALL(*key.get(), Sign(_, _, _))
      .Times(0);
  pol.StoreOwnerProperties(key.get(), current_user, NULL);

  ASSERT_EQ(CountOwnerInWhitelist(pol, current_user), 1);
  ASSERT_TRUE(AreNewUsersAllowed(pol));
}

TEST_F(DevicePolicyTest, ExistingPolicy) {
  StartFresh();
  DevicePolicy pol(tmpfile_);
  ASSERT_TRUE(pol.LoadOrCreate());  // Should create an empty policy.

  std::string current_user("me");
  const char* users[] = { "you", "him", "her" };
  std::vector<std::string> default_whitelist(users, users + arraysize(users));
  pol.Set(CreateWithWhitelist(default_whitelist));

  scoped_ptr<MockOwnerKey> key(new MockOwnerKey);
  EXPECT_CALL(*key.get(), Sign(_, _, _))
      .WillOnce(Return(true));
  pol.StoreOwnerProperties(key.get(), current_user, NULL);

  ASSERT_EQ(CountOwnerInWhitelist(pol, current_user), 1);
}

}  // namespace login_manager
