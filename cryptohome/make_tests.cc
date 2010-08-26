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
#include "mount.h"
#include "secure_blob.h"
#include "username_passkey.h"
#include "vault_keyset.h"

namespace cryptohome {

// struct TestUserInfo {
//   const char* username;
//   const char* password;
//   bool create;
//   bool use_old_format;
// };
const TestUserInfo kDefaultUsers[] = {
  {"testuser0@invalid.domain", "zero", true, false},
  {"testuser1@invalid.domain", "one", true, false},
  {"testuser2@invalid.domain", "two", true, false},
  {"testuser3@invalid.domain", "three", true, false},
  {"testuser4@invalid.domain", "four", true, false},
  {"testuser5@invalid.domain", "five", false, false},
  {"testuser6@invalid.domain", "six", true, false},
  {"testuser7@invalid.domain", "seven", true, true},
  {"testuser8@invalid.domain", "eight", true, false},
};
const unsigned int kDefaultUserCount =
    sizeof(kDefaultUsers) / sizeof(kDefaultUsers[0]);

MakeTests::MakeTests() {
}

void MakeTests::InitTestData(const std::string& image_dir) {
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
  for (unsigned int i = 0; i < kDefaultUserCount; i++) {
    if (kDefaultUsers[i].create) {
      Mount mount;
      mount.set_shadow_root(image_dir);
      mount.set_skel_source(StringPrintf("%s/skel", image_dir.c_str()));

      mount.set_use_tpm(false);
      mount.set_fallback_to_scrypt(false);
      mount.Init();

      cryptohome::SecureBlob passkey;
      cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[i].password,
                                            salt,
                                            &passkey);
      UsernamePasskey up(kDefaultUsers[i].username, passkey);
      bool created;
      mount.EnsureCryptohome(up, &created);

      if (kDefaultUsers[i].use_old_format) {
        VaultKeyset vault_keyset;
        cryptohome::Mount::MountError error;
        if (mount.UnwrapVaultKeyset(up, false, &vault_keyset, &error)) {
          mount.RemoveOldFiles(up);
          mount.SaveVaultKeysetOld(up, vault_keyset);
        }
      }
    }
  }
}

} // namespace cryptohome
