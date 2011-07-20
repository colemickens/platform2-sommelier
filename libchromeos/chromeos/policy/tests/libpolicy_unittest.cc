// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_path.h>
#include <base/logging.h>
#include <gtest/gtest.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <chromeos/policy/libpolicy.h>
#include <chromeos/policy/device_policy_impl.h>

namespace policy {

static const char kPolicyFile[] = "chromeos/policy/tests/whitelist/policy";
static const char kKeyFile[] = "chromeos/policy/tests/whitelist/owner.key";

// This class mocks only the minimally needed functionionallity to run tests
// that would otherwise fail because of hard restrictions like root file
// ownership. But else preserves all the functionallity of the original class.
class MockDevicePolicyImpl : public DevicePolicyImpl {
 public:
  MockDevicePolicyImpl(const FilePath& policy_path,
                       const FilePath& keyfile_path,
                       bool verify_files)
      : verify_files_(verify_files_) {
    policy_path_ = policy_path;
    keyfile_path_ = keyfile_path;
  }

 private:
  // We don't care if files are owned by root for most tests.
  virtual bool VerifyPolicyFiles() {
    return !verify_files_ || DevicePolicyImpl::VerifyPolicyFiles();
  }

  bool verify_files_;
};

// Test if a policy file can be verified and parsed correctly.
TEST(PolicyTest, DevicePolicyTest) {
  FilePath policy_file(kPolicyFile);
  FilePath key_file(kKeyFile);
  MockDevicePolicyImpl* device_policy =
      new MockDevicePolicyImpl(policy_file, key_file, false);
  PolicyProvider provider(device_policy);
  provider.Reload();

  // Ensure we got devcie policy and fetch it.
  ASSERT_TRUE(provider.device_policy_is_loaded());

  const DevicePolicy& policy = provider.GetDevicePolicy();

  // Check if we can read out all fields of the sample protobuf.
  bool bool_value = true;
  ASSERT_TRUE(policy.GetGuestModeEnabled(&bool_value));
  ASSERT_EQ(false, bool_value);

  int int_value = -1;
  ASSERT_TRUE(policy.GetPolicyRefreshRate(&int_value));
  ASSERT_EQ(100, int_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetCameraEnabled(&bool_value));
  ASSERT_EQ(false, bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetShowUserNames(&bool_value));
  ASSERT_EQ(false, bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetDataRoamingEnabled(&bool_value));
  ASSERT_EQ(false, bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetAllowNewUsers(&bool_value));
  ASSERT_EQ(false, bool_value);

  std::vector<std::string> list_value;
  ASSERT_TRUE(policy.GetUserWhitelist(&list_value));
  ASSERT_EQ(3, list_value.size());
  ASSERT_EQ("me@here.com", list_value[0]);
  ASSERT_EQ("you@there.com", list_value[1]);
  ASSERT_EQ("*@monsters.com", list_value[2]);

  std::string string_value;
  ASSERT_TRUE(policy.GetProxyMode(&string_value));
  ASSERT_EQ("direct", string_value);

  ASSERT_TRUE(policy.GetProxyServer(&string_value));
  ASSERT_EQ("myproxy", string_value);

  ASSERT_TRUE(policy.GetProxyPacUrl(&string_value));
  ASSERT_EQ("http://mypac.pac", string_value);

  ASSERT_TRUE(policy.GetProxyBypassList(&string_value));
  ASSERT_EQ("a, b, c", string_value);

  // Test one missing policy.
  ASSERT_TRUE(policy.GetMetricsEnabled(&bool_value));
  ASSERT_EQ(false, bool_value);

  // Try reloading the protobuf should succeed.
  ASSERT_TRUE(provider.Reload());
}

// Verify that the library will correctly recognize and signal missing files.
TEST(PolicyTest, DevicePolicyFailure) {
  LOG(INFO) << "Errors expected.";
  // Try loading non-existing protobuf should fail.
  FilePath non_existing("this_file_is_doof");
  MockDevicePolicyImpl* device_policy =
      new MockDevicePolicyImpl(non_existing, non_existing, true);
  PolicyProvider provider(device_policy);
  // Even after reload the policy should still be not loaded.
  ASSERT_FALSE(provider.Reload());
  ASSERT_FALSE(provider.device_policy_is_loaded());
}

}  // namespace policy

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
