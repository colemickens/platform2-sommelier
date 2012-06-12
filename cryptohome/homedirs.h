// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Homedirs - manages the collection of user home directories on disk. When a
// homedir is actually mounted, it becomes a Mount.

#ifndef HOMEDIRS_H_
#define HOMEDIRS_H_

#include <base/callback.h>
#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <chaps/login_event_client.h>
#include <chromeos/secure_blob.h>
#include <policy/device_policy.h>
#include <policy/libpolicy.h>
#include <string>

#include "crypto.h"
#include "vault_keyset.pb.h"

namespace cryptohome {

const int64 kMinFreeSpace = 512 * 1LL << 20;
const int64 kEnoughFreeSpace = 1LL << 30;

extern const base::TimeDelta kOldUserLastActivityTime;

class Credentials;
class Platform;
class UserOldestActivityTimestampCache;

class HomeDirs {
 public:
  HomeDirs();
  virtual ~HomeDirs();

  // Initializes this HomeDirs object. Returns true for success.
  virtual bool Init();

  // Frees disk space for unused cryptohomes. If less than kMinFreeSpace is
  // available, frees space until kEnoughFreeSpace is available. Returns true if
  // there is now at least kEnoughFreeSpace, or false otherwise.
  virtual bool FreeDiskSpace();

  // Removes all cryptohomes owned by anyone other than the owner user (if set),
  // regardless of free disk space.
  virtual void RemoveNonOwnerCryptohomes();

  // Returns the system salt, creating a new one if necessary. If loading the
  // system salt fails, returns false, and blob is unchanged.
  virtual bool GetSystemSalt(chromeos::SecureBlob *blob);

  // Returns the owner's obfuscated username.
  virtual bool GetOwner(std::string* owner);

  // Removes the cryptohome for the named user.
  virtual bool Remove(const std::string& username);

  // Returns true if the supplied Credentials are a valid (username, passkey)
  // pair.
  virtual bool AreCredentialsValid(const Credentials& credentials);

  // Returns the vault keyset path for the supplied obfuscated username.
  virtual std::string GetVaultKeysetPath(const std::string& obfuscated) const;

  // Migrates the cryptohome for the supplied obfuscated username from the
  // supplied old key to the supplied new key.
  virtual bool Migrate(const Credentials& newcreds,
                       const chromeos::SecureBlob& oldkey);

  // Accessors. Mostly used for unit testing. These do not take ownership of
  // passed-in pointers.
  void set_platform(Platform *value) { platform_ = value; }
  Platform* platform() { return platform_; }
  void set_shadow_root(const std::string& value) { shadow_root_ = value; }
  const std::string& shadow_root() const { return shadow_root_; }
  UserOldestActivityTimestampCache *timestamp_cache() {
    return timestamp_cache_;
  }
  void set_timestamp_cache(UserOldestActivityTimestampCache *value) {
    timestamp_cache_ = value;
  }
  void set_enterprise_owned(bool value) { enterprise_owned_ = value; }
  bool enterprise_owned() const { return enterprise_owned_; }
  void set_old_user_last_activity_time(base::TimeDelta value) {
    old_user_last_activity_time_ = value;
  }
  const base::TimeDelta &old_user_last_activity_time() const {
    return old_user_last_activity_time_;
  }
  void set_policy_provider(policy::PolicyProvider* value) {
    policy_provider_ = value;
  }
  policy::PolicyProvider* policy_provider() { return policy_provider_; }
  void set_crypto(Crypto* value) { crypto_ = value; }
  Crypto* crypto() const { return crypto_; }

 private:
  bool AreEphemeralUsersEnabled();
  // Loads the device policy, either by initializing it or reloading the
  // existing one.
  void LoadDevicePolicy();
  typedef base::Callback<void(const FilePath&)> CryptohomeCallback;
  // Runs the supplied callback for every unmounted cryptohome.
  void DoForEveryUnmountedCryptohome(const CryptohomeCallback& cryptohome_cb);
  // Callback used during RemoveNonOwnerCryptohomes()
  void RemoveNonOwnerCryptohomesCallback(const FilePath& vault);
  // Callback used during FreeDiskSpace().
  void DeleteCacheCallback(const FilePath& vault);
  // Callback used during FreeDiskSpace().
  void DeleteGCacheTmpCallback(const FilePath& vault);
  // Recursively deletes all contents of a directory while leaving the directory
  // itself intact.
  void DeleteDirectoryContents(const FilePath& dir);
  // Deletes all directories under the supplied directory whose basename is not
  // the same as the obfuscated owner name.
  void RemoveNonOwnerDirectories(const FilePath& prefix);
  // Callback used during FreeDiskSpace()
  void AddUserTimestampToCacheCallback(const FilePath& vault);
  // Loads the serialized vault keyset for the supplied obfuscated username.
  // Returns true for success, false for failure.
  bool LoadVaultKeysetForUser(const std::string& obfuscated_user,
                              SerializedVaultKeyset* serialized) const;

  // Takes ownership of the supplied PolicyProvider. Used to avoid leaking mocks
  // in unit tests.
  void own_policy_provider(policy::PolicyProvider* value) {
    default_policy_provider_.reset(value);
    policy_provider_ = value;
  }

  scoped_ptr<Platform> default_platform_;
  Platform* platform_;
  std::string shadow_root_;
  scoped_ptr<UserOldestActivityTimestampCache> default_timestamp_cache_;
  UserOldestActivityTimestampCache* timestamp_cache_;
  bool enterprise_owned_;
  scoped_ptr<policy::PolicyProvider> default_policy_provider_;
  policy::PolicyProvider* policy_provider_;
  scoped_ptr<Crypto> default_crypto_;
  Crypto* crypto_;
  base::TimeDelta old_user_last_activity_time_;
  chromeos::SecureBlob system_salt_;
  chaps::LoginEventClient chaps_event_client_;

  friend class HomeDirsTest;

  DISALLOW_COPY_AND_ASSIGN(HomeDirs);
};

}  // namespace cryptohome

#endif  // HOMEDIRS_H_
