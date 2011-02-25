// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates credential stores for testing

#include "make_tests.h"

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/utility.h>

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
//   bool use_old_format;
//   const char* tracked_dirs[2];
// };

const TestUserInfo kDefaultUsers[] = {
  {"testuser0@invalid.domain", "zero", true, false, {NULL}},
  {"testuser1@invalid.domain", "one", true, false, {NULL}},
  {"testuser2@invalid.domain", "two", true, false, {NULL}},
  {"testuser3@invalid.domain", "three", true, false, {NULL}},
  {"testuser4@invalid.domain", "four", true, false, {NULL}},
  {"testuser5@invalid.domain", "five", false, false, {NULL}},
  {"testuser6@invalid.domain", "six", true, false, {NULL}},
  {"testuser7@invalid.domain", "seven", true, true, {NULL}},
  {"testuser8@invalid.domain", "eight", true, false, {NULL}},
  {"testuser9@invalid.domain", "nine", true, false, {"DIR0", NULL}},
  {"testuser10@invalid.domain", "ten", true, false, {"DIR0", NULL}},
  {"testuser11@invalid.domain", "eleven", true, false, {"DIR0", NULL}},
  {"testuser12@invalid.domain", "twelve", false, false, {"DIR0", NULL}},
};
const size_t kDefaultUserCount =
    sizeof(kDefaultUsers) / sizeof(kDefaultUsers[0]);

const TestUserInfo kAlternateUsers[] = {
  {"altuser0@invalid.domain", "zero", true, false, {"DIR0", NULL}},
  {"altuser1@invalid.domain", "odin", true, false},
};
const size_t kAlternateUserCount =
    sizeof(kAlternateUsers) / sizeof(kAlternateUsers[0]);

MakeTests::MakeTests() {
}

void MakeTests::InitTestData(const std::string& image_dir,
                             const TestUserInfo* test_users,
                             size_t test_user_count) {
  if (file_util::PathExists(FilePath(image_dir))) {
    file_util::Delete(FilePath(image_dir), true);
  }

  file_util::CreateDirectory(FilePath(image_dir));

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

  // Create the user credentials
  for (unsigned int i = 0; i < test_user_count; i++) {
    if (test_users[i].create) {
      Mount mount;
      NiceMock<MockPlatform> platform;
      mount.set_platform(&platform);
      NiceMock<MockTpm> tpm;
      mount.get_crypto()->set_tpm(&tpm);
      mount.set_shadow_root(image_dir);
      mount.set_skel_source(StringPrintf("%s/skel", image_dir.c_str()));

      mount.set_use_tpm(false);
      mount.set_fallback_to_scrypt(false);
      mount.Init();

      cryptohome::SecureBlob passkey;
      cryptohome::Crypto::PasswordToPasskey(test_users[i].password,
                                            salt,
                                            &passkey);
      UsernamePasskey up(test_users[i].username, passkey);
      bool created;
      Mount::MountArgs mount_args;
      if (test_users[i].tracked_dirs[0] != NULL) {
        mount_args.AssignSubdirsNullTerminatedList(
            const_cast<const char**>(test_users[i].tracked_dirs));
      }
      mount.EnsureCryptohome(up, mount_args, &created);

      if (test_users[i].use_old_format) {
        VaultKeyset vault_keyset;
        SerializedVaultKeyset serialized;
        cryptohome::Mount::MountError error;
        if (mount.DecryptVaultKeyset(up, false, &vault_keyset, &serialized,
                                     &error)) {
          mount.RemoveOldFiles(up);
          mount.SaveVaultKeysetOld(up, vault_keyset);
        }
      }
    }
  }
}

} // namespace cryptohome
