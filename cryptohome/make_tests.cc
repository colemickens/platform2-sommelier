// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates credential stores for testing

#include "make_tests.h"

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/cryptohome.h>
#include <chromeos/utility.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "crypto.h"
#include "mock_platform.h"
#include "mock_tpm.h"
#include "mount.h"
#include "secure_blob.h"
#include "username_passkey.h"
#include "vault_keyset.h"

namespace cryptohome {
using ::testing::NiceMock;

// struct TestUserInfo {
//   const char* username;
//   const char* password;
//   bool create;
// };

const TestUserInfo kDefaultUsers[] = {
  {"testuser0@invalid.domain", "zero", true},
  {"testuser1@invalid.domain", "one", true},
  {"testuser2@invalid.domain", "two", true},
  {"testuser3@invalid.domain", "three", true},
  {"testuser4@invalid.domain", "four", true},
  {"testuser5@invalid.domain", "five", false},
  {"testuser6@invalid.domain", "six", true},
  {"testuser7@invalid.domain", "seven", true},
  {"testuser8@invalid.domain", "eight", true},
  {"testuser9@invalid.domain", "nine", true},
  {"testuser10@invalid.domain", "ten", true},
  {"testuser11@invalid.domain", "eleven", true},
  {"testuser12@invalid.domain", "twelve", false},
  {"testuser13@invalid.domain", "thirteen", true},
};
const size_t kDefaultUserCount = arraysize(kDefaultUsers);
const char kUserHomeDir[] = "test_user_home";
const char kRootHomeDir[] = "test_root_home";

MakeTests::MakeTests() {
}

void MakeTests::InitTestData(const std::string& image_dir,
                             const TestUserInfo* test_users,
                             size_t test_user_count) {
  FilePath user_dir(kUserHomeDir);
  FilePath root_dir(kRootHomeDir);
  file_util::Delete(FilePath(image_dir), true);
  file_util::Delete(user_dir, true);
  file_util::Delete(root_dir, true);

  file_util::CreateDirectory(FilePath(image_dir));
  file_util::CreateDirectory(user_dir);
  file_util::CreateDirectory(root_dir);

  std::string skeleton_path = StringPrintf("%s/skel/sub_path",
                                           image_dir.c_str());
  file_util::CreateDirectory(FilePath(skeleton_path));
  std::string skeleton_file = StringPrintf("%s/.testfile",
                                           skeleton_path.c_str());
  file_util::WriteFile(FilePath(skeleton_file), "test", 4);

  Crypto crypto;
  SecureBlob salt;
  FilePath salt_path(StringPrintf("%s/salt", image_dir.c_str()));
  crypto.GetOrCreateSalt(salt_path, 16, true, &salt);

  chromeos::cryptohome::home::SetUserHomePrefix(user_dir.value() + "/");
  chromeos::cryptohome::home::SetRootHomePrefix(root_dir.value() + "/");
  chromeos::cryptohome::home::SetSystemSaltPath(salt_path.value());

  // Create the user credentials
  for (unsigned int i = 0; i < test_user_count; i++) {
    if (!test_users[i].create)
      continue;
    Mount mount;
    NiceMock<MockPlatform> platform;
    mount.set_platform(&platform);
    NiceMock<MockTpm> tpm;
    mount.get_crypto()->set_tpm(&tpm);
    mount.set_shadow_root(image_dir);
    mount.set_skel_source(StringPrintf("%s/skel", image_dir.c_str()));

    mount.set_use_tpm(false);
    mount.set_policy_provider(new policy::PolicyProvider(
        new NiceMock<policy::MockDevicePolicy>));
    mount.Init();

    cryptohome::SecureBlob passkey;
    cryptohome::Crypto::PasswordToPasskey(test_users[i].password,
                                          salt,
                                          &passkey);
    UsernamePasskey up(test_users[i].username, passkey);
    bool created;
    mount.EnsureCryptohome(up, &created);
  }
}

}  // namespace cryptohome
