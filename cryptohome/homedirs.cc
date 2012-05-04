// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/logging.h>
#include <chromeos/cryptohome.h>

#include "credentials.h"
#include "homedirs.h"
#include "mount.h"
#include "platform.h"
#include "username_passkey.h"
#include "user_oldest_activity_timestamp_cache.h"
#include "vault_keyset.h"

namespace cryptohome {

const char *kShadowRoot = "/home/.shadow";
const char *kEmptyOwner = "";

HomeDirs::HomeDirs()
    : default_platform_(new Platform()),
      platform_(default_platform_.get()),
      shadow_root_(kShadowRoot),
      default_timestamp_cache_(new UserOldestActivityTimestampCache()),
      timestamp_cache_(default_timestamp_cache_.get()),
      enterprise_owned_(false),
      default_policy_provider_(new policy::PolicyProvider()),
      policy_provider_(default_policy_provider_.get()),
      default_crypto_(new Crypto()),
      crypto_(default_crypto_.get()) { }

HomeDirs::~HomeDirs() { }

bool HomeDirs::Init() {
  if (!platform_->DirectoryExists(shadow_root_))
    platform_->CreateDirectory(shadow_root_);
  return GetSystemSalt(NULL);
}

bool HomeDirs::FreeDiskSpace() {
  if (platform_->AmountOfFreeDiskSpace(shadow_root_) > kMinFreeSpace) {
    return false;
  }

  LoadDevicePolicy();

  // If ephemeral users are enabled, remove all cryptohomes except those
  // currently mounted or belonging to the owner.
  if (AreEphemeralUsersEnabled()) {
    RemoveNonOwnerCryptohomes();
    return true;
  }

  // Clean Cache directories for every user (except current one).
  DoForEveryUnmountedCryptohome(base::Bind(&HomeDirs::DeleteCacheCallback,
                                           base::Unretained(this)));

  if (platform_->AmountOfFreeDiskSpace(shadow_root_) >= kEnoughFreeSpace)
    return true;

  // Clean Cache directories for every user (except current one).
  DoForEveryUnmountedCryptohome(base::Bind(&HomeDirs::DeleteGCacheTmpCallback,
                                           base::Unretained(this)));

  if (platform_->AmountOfFreeDiskSpace(shadow_root_) >= kEnoughFreeSpace)
    return true;

  // Initialize user timestamp cache if it has not been yet.  Current
  // user is not added now, but added on log out or during daily
  // updates (UpdateCurrentUserActivityTimestamp()).
  if (!timestamp_cache_->initialized()) {
    timestamp_cache_->Initialize();
    DoForEveryUnmountedCryptohome(base::Bind(
        &HomeDirs::AddUserTimestampToCacheCallback,
        base::Unretained(this)));
  }

  // Delete old users, the oldest first.
  // Don't delete anyone if we don't know who the owner is.
  std::string owner;
  if (enterprise_owned_ || GetOwner(&owner)) {
    const base::Time timestamp_threshold =
        base::Time::Now() - old_user_last_activity_time_;
    while (!timestamp_cache_->oldest_known_timestamp().is_null() &&
           timestamp_cache_->oldest_known_timestamp() <= timestamp_threshold) {
      FilePath deleted_user_dir = timestamp_cache_->RemoveOldestUser();
      if (!enterprise_owned_) {
        std::string obfuscated_username = deleted_user_dir.BaseName().value();
        if (obfuscated_username == owner)
          continue;
      }
      std::string mountdir = deleted_user_dir.Append(kMountDir).value();
      std::string vaultdir = deleted_user_dir.Append(kVaultDir).value();
      if (platform_->IsDirectoryMountedWith(mountdir, vaultdir)) {
        LOG(INFO) << "Attempt to delete currently logged user. Skipped...";
      } else {
        LOG(INFO) << "Deleting old user " << deleted_user_dir.value();
        platform_->DeleteFile(deleted_user_dir.value(), true);
        if (platform_->AmountOfFreeDiskSpace(shadow_root_) >= kEnoughFreeSpace)
          return true;
      }
    }
  }

  // TODO(glotov): do further cleanup.
  return true;
}

void HomeDirs::LoadDevicePolicy() {
  policy_provider_->Reload();
}

bool HomeDirs::AreEphemeralUsersEnabled() {
  LoadDevicePolicy();
  // If the policy cannot be loaded, default to non-ephemeral users.
  bool ephemeral_users_enabled = false;
  if (policy_provider_->device_policy_is_loaded())
    policy_provider_->GetDevicePolicy().GetEphemeralUsersEnabled(
        &ephemeral_users_enabled);
  return ephemeral_users_enabled;
}

bool HomeDirs::AreCredentialsValid(const Credentials& creds) {
  LoadDevicePolicy();
  std::string owner;
  std::string obfuscated = creds.GetObfuscatedUsername(system_salt_);
  if (AreEphemeralUsersEnabled() && GetOwner(&owner) && obfuscated != owner)
    return false;
  VaultKeyset vk(platform_, crypto_);
  SecureBlob passkey;
  creds.GetPasskey(&passkey);
  std::string path = GetVaultKeysetPath(obfuscated);
  return vk.Load(GetVaultKeysetPath(obfuscated), passkey);
}

std::string HomeDirs::GetVaultKeysetPath(const std::string& obfuscated) const {
  FilePath path(shadow_root_);
  return path.Append(obfuscated).Append(kKeyFile).value();
}

void HomeDirs::RemoveNonOwnerCryptohomesCallback(const FilePath& vault) {
  std::string owner;
  if (!GetOwner(&owner))
    return;
  if (vault != FilePath(shadow_root_).Append(owner).Append(kVaultDir))
    platform_->DeleteFile(vault.DirName().value(), true);
}

void HomeDirs::RemoveNonOwnerCryptohomes() {
  std::string owner;
  if (!enterprise_owned_ && !GetOwner(&owner))
    return;

  DoForEveryUnmountedCryptohome(base::Bind(
      &HomeDirs::RemoveNonOwnerCryptohomesCallback,
      base::Unretained(this)));
  // TODO(ellyjones): is this valuable? These two directories should just be
  // mountpoints.
  RemoveNonOwnerDirectories(chromeos::cryptohome::home::GetUserPathPrefix());
  RemoveNonOwnerDirectories(chromeos::cryptohome::home::GetRootPathPrefix());
}


void HomeDirs::DoForEveryUnmountedCryptohome(
    const CryptohomeCallback& cryptohome_cb) {
  std::vector<std::string> entries;
  if (!platform_->EnumerateDirectoryEntries(shadow_root_, false, &entries))
    return;
  for (std::vector<std::string>::iterator it = entries.begin();
       it != entries.end(); ++it) {
    FilePath path(*it);
    const std::string dir_name = path.BaseName().value();
    if (!chromeos::cryptohome::home::IsSanitizedUserName(dir_name))
      continue;
    std::string vault_path = path.Append(kVaultDir).value();
    std::string mount_path = path.Append(kMountDir).value();
    if (!platform_->DirectoryExists(vault_path)) {
      continue;
    }
    if (platform_->IsDirectoryMountedWith(mount_path, vault_path)) {
      continue;
    }
    cryptohome_cb.Run(FilePath(vault_path));
  }
}

void HomeDirs::DeleteDirectoryContents(const FilePath& dir) {
  file_util::FileEnumerator subdir_enumerator(
      dir,
      false,
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::SHOW_SYM_LINKS));
  for (FilePath subdir_path = subdir_enumerator.Next(); !subdir_path.empty();
       subdir_path = subdir_enumerator.Next()) {
    platform_->DeleteFile(subdir_path.value(), true);
  }
}

void HomeDirs::RemoveNonOwnerDirectories(const FilePath& prefix) {
  std::vector<std::string> dirents;
  if (!platform_->EnumerateDirectoryEntries(prefix.value(), false, &dirents))
    return;
  std::string owner;
  if (!GetOwner(&owner))
    return;
  for (std::vector<std::string>::iterator it = dirents.begin();
       it != dirents.end(); ++it) {
    FilePath path(*it);
    const std::string basename = path.BaseName().value();
    if (!strcasecmp(basename.c_str(), owner.c_str()))
      continue;  // Skip the owner's directory.
    if (!chromeos::cryptohome::home::IsSanitizedUserName(basename))
      continue;  // Skip any directory whose name is not an obfuscated user
                 // name.
    if (platform_->IsDirectoryMounted(path.value()))
      continue;  // Skip any directory that is currently mounted.
    platform_->DeleteFile(path.value(), true);
  }
}

void HomeDirs::DeleteCacheCallback(const FilePath& vault) {
  const FilePath cache = vault.Append(kUserHomeSuffix).Append(kCacheDir);
  LOG(WARNING) << "Deleting Cache " << cache.value();
  DeleteDirectoryContents(cache);
}

void HomeDirs::DeleteGCacheTmpCallback(const FilePath& vault) {
  const FilePath gcachetmp = vault.Append(kUserHomeSuffix).Append(kGCacheDir)
      .Append(kGCacheVersionDir).Append(kGCacheTmpDir);
  LOG(WARNING) << "Deleting GCache " << gcachetmp.value();
  DeleteDirectoryContents(gcachetmp);
}

void HomeDirs::AddUserTimestampToCacheCallback(const FilePath& vault) {
  const FilePath user_dir = vault.DirName();
  const std::string obfuscated_username = user_dir.BaseName().value();
  SerializedVaultKeyset serialized;
  LoadVaultKeysetForUser(obfuscated_username, &serialized);
  if (serialized.has_last_activity_timestamp()) {
    const base::Time timestamp = base::Time::FromInternalValue(
        serialized.last_activity_timestamp());
    timestamp_cache_->AddExistingUser(user_dir, timestamp);
  } else {
    timestamp_cache_->AddExistingUserNotime(user_dir);
  }
}

bool HomeDirs::LoadVaultKeysetForUser(const std::string& obfuscated_user,
                                      SerializedVaultKeyset* serialized) const {
  // Load the encrypted keyset
  FilePath user_key_file(shadow_root_);
  user_key_file = user_key_file.Append(obfuscated_user).Append(kKeyFile);
  if (!file_util::PathExists(user_key_file))
    return false;
  SecureBlob cipher_text;
  if (!LoadFileBytes(user_key_file, &cipher_text))
    return false;
  if (!serialized->ParseFromArray(
           static_cast<const unsigned char*>(cipher_text.data()),
           cipher_text.size())) {
    return false;
  }
  return true;
}

bool HomeDirs::LoadFileBytes(const FilePath& path, SecureBlob* blob) const {
  bool ok = platform_->ReadFile(path.value(), blob);
  if (!ok)
    LOG(ERROR) << "Could not read " << path.value();
  return ok;
}

bool HomeDirs::GetOwner(std::string* owner) {
  std::string plain_owner;
  if (!policy_provider_->device_policy_is_loaded())
    return false;
  policy_provider_->GetDevicePolicy().GetOwner(&plain_owner);

  if (plain_owner.empty())
    return false;

  if (!GetSystemSalt(NULL))
    return false;
  *owner = UsernamePasskey(plain_owner.c_str(), chromeos::Blob())
      .GetObfuscatedUsername(system_salt_);
  return true;
}

bool HomeDirs::GetSystemSalt(SecureBlob* blob) {
  FilePath salt_file = FilePath(shadow_root_).Append("salt");
  if (!crypto_->GetOrCreateSalt(salt_file, CRYPTOHOME_DEFAULT_SALT_LENGTH,
                                false, &system_salt_)) {
    LOG(ERROR) << "Failed to create system salt.";
    return false;
  }
  if (blob)
    *blob = system_salt_;
  return true;
}

bool HomeDirs::Remove(const std::string& username) {
  UsernamePasskey passkey(username.c_str(), SecureBlob("", 0));
  std::string obfuscated = passkey.GetObfuscatedUsername(system_salt_);
  FilePath user_dir = FilePath(shadow_root_).Append(obfuscated);
  FilePath user_path = chromeos::cryptohome::home::GetUserPath(username);
  FilePath root_path = chromeos::cryptohome::home::GetRootPath(username);
  return platform_->DeleteFile(user_dir.value(), true) &&
         platform_->DeleteFile(user_path.value(), true) &&
         platform_->DeleteFile(root_path.value(), true);
}

bool HomeDirs::Migrate(const Credentials& newcreds,
                       const SecureBlob& oldkey) {
  SecureBlob newkey;
  newcreds.GetPasskey(&newkey);
  UsernamePasskey oldcreds(newcreds.username().c_str(), oldkey);
  Mount mount;
  mount.set_platform(platform_);
  mount.set_crypto(crypto_);
  mount.Init();
  if (!mount.MountCryptohome(oldcreds, Mount::MountArgs(), NULL)) {
    LOG(ERROR) << "Migrate: Mount failed";
    // Fail as early as possible. Note that we don't have to worry about leaking
    // this mount - Mount unmounts itself if it's still mounted in the
    // destructor.
    return false;
  }
  VaultKeyset keyset(platform_, crypto_);
  std::string path = GetVaultKeysetPath(
      newcreds.GetObfuscatedUsername(system_salt_));
  if (!keyset.Load(path, oldkey)) {
    LOG(ERROR) << "Can't load vault keyset at " << path;
    return false;
  }
  if (!keyset.Save(path, newkey)) {
    LOG(ERROR) << "Can't save vault keyset at " << path;
    return false;
  }
  SecureBlob old_auth_data;
  SecureBlob auth_data;
  FilePath salt_file(kTokenSaltFile);
  if (!crypto_->PasskeyToTokenAuthData(newkey, salt_file, &auth_data))
    return false;
  if (!crypto_->PasskeyToTokenAuthData(oldkey, salt_file, &old_auth_data))
    return false;
  chaps_event_client_.FireChangeAuthDataEvent(
      kChapsTokenDir,
      static_cast<const uint8_t*>(old_auth_data.const_data()),
      old_auth_data.size(),
      static_cast<const uint8_t*>(auth_data.const_data()),
      auth_data.size());
  return true;
}

}  // namespace cryptohome
