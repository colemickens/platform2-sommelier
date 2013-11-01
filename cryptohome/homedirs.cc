// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/cryptohome.h>

#include "credentials.h"
#include "homedirs.h"
#include "mount.h"
#include "platform.h"
#include "username_passkey.h"
#include "user_oldest_activity_timestamp_cache.h"
#include "vault_keyset.h"

using chromeos::SecureBlob;

namespace cryptohome {

const char *kShadowRoot = "/home/.shadow";
const char *kEmptyOwner = "";

const char *kRemoveLRU = "remove-lru";;
const char *kRemoveLRUIfDormant = "remove-lru-if-dormant";

HomeDirs::HomeDirs()
    : default_platform_(new Platform()),
      platform_(default_platform_.get()),
      shadow_root_(kShadowRoot),
      default_timestamp_cache_(new UserOldestActivityTimestampCache()),
      timestamp_cache_(default_timestamp_cache_.get()),
      enterprise_owned_(false),
      default_policy_provider_(new policy::PolicyProvider()),
      policy_provider_(default_policy_provider_.get()),
      default_crypto_(new Crypto(platform_)),
      crypto_(default_crypto_.get()),
      default_mount_factory_(new MountFactory()),
      mount_factory_(default_mount_factory_.get()),
      default_vault_keyset_factory_(new VaultKeysetFactory()),
      vault_keyset_factory_(default_vault_keyset_factory_.get()) { }

HomeDirs::~HomeDirs() { }

bool HomeDirs::Init() {
  LoadDevicePolicy();
  if (!platform_->DirectoryExists(shadow_root_))
    platform_->CreateDirectory(shadow_root_);
  return GetSystemSalt(NULL);
}

bool HomeDirs::FreeDiskSpace() {
  if (platform_->AmountOfFreeDiskSpace(shadow_root_) > kMinFreeSpace) {
    return false;
  }

  // If ephemeral users are enabled, remove all cryptohomes except those
  // currently mounted or belonging to the owner.
  // |AreEphemeralUsers| will reload the policy to guarantee freshness.
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
        base::Time::Now() - GetUserInactivityThresholdForRemoval();
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

base::TimeDelta HomeDirs::GetUserInactivityThresholdForRemoval() {
  LoadDevicePolicy();
  // If the policy cannot be loaded, default to LRU-if-dormant strategy.
  std::string strategy = kRemoveLRUIfDormant;
  if (policy_provider_->device_policy_is_loaded()) {
    policy_provider_->GetDevicePolicy().GetCleanUpStrategy(&strategy);
  }
  if (strategy == kRemoveLRUIfDormant)
    return old_user_last_activity_time_;
  else if (strategy == kRemoveLRU)
    return base::TimeDelta();
  else
    LOG(ERROR) << "Unknown strategy : " << strategy;
  return old_user_last_activity_time_;
}

bool HomeDirs::AreCredentialsValid(const Credentials& creds) {
  scoped_ptr<VaultKeyset> vk(vault_keyset_factory()->New(platform_, crypto_));
  return GetValidKeyset(creds, vk.get());
}

bool HomeDirs::GetValidKeyset(const Credentials& creds, VaultKeyset* vk) {
  if (!vk)
    return false;

  std::string owner;
  std::string obfuscated = creds.GetObfuscatedUsername(system_salt_);
  // |AreEphemeralUsers| will reload the policy to guarantee freshness.
  if (AreEphemeralUsersEnabled() && GetOwner(&owner) && obfuscated != owner)
    return false;

  std::vector<int> key_indices;
  if (!GetVaultKeysets(obfuscated, &key_indices)) {
    LOG(WARNING) << "No valid keysets on disk for " << obfuscated;
    return false;
  }

  SecureBlob passkey;
  creds.GetPasskey(&passkey);

  std::vector<int>::const_iterator iter = key_indices.begin();
  for ( ;iter != key_indices.end(); ++iter) {
    if (vk->Load(GetVaultKeysetPath(obfuscated, *iter)) &&
        vk->Decrypt(passkey))
      return true;
  }
  return false;
}

// TODO(wad) Figure out how this might fit in with vault_keyset.cc
bool HomeDirs::GetVaultKeysets(const std::string& obfuscated,
                               std::vector<int>* keysets) const {
  CHECK(keysets);
  std::string user_dir = FilePath(shadow_root_).Append(obfuscated).value();

  scoped_ptr<FileEnumerator> file_enumerator(
      platform_->GetFileEnumerator(user_dir, false,
                                   FileEnumerator::FILES));
  std::string next_path;
  while (!(next_path = file_enumerator->Next()).empty()) {
    std::string file_name = FilePath(next_path).BaseName().value();
    // Scan for "master." files.
    if (file_name.find(kKeyFile, 0, strlen(kKeyFile) == std::string::npos))
      continue;
    char *end = NULL;
    std::string index_str = file_name.substr(strlen(kKeyFile));
    const char * index_c_str = index_str.c_str();
    long index = strtol(index_c_str, &end, 10);
    // Ensure the entire suffix is consumed.
    if (end && *end != '\0')
      continue;
    // The test below will catch all strtol(3) error conditions.
    if (index < 0 || index >= kKeyFileMax) {
      LOG(ERROR) << "Invalid key file range: " << index;
      continue;
    }
    keysets->push_back(static_cast<int>(index));
  }

  // Ensure it is sorted numerically and not lexigraphically.
  std::sort(keysets->begin(), keysets->end());

  return keysets->size() != 0;
}

bool HomeDirs::AddKeyset(const Credentials& existing_credentials,
                         const SecureBlob& new_passkey,
                         int* index) {
  // TODO(wad) Determine how to best bubble up the failures MOUNT_ERROR
  //           encapsulate wrt the TPM behavior.
  std::string obfuscated = existing_credentials.GetObfuscatedUsername(
    system_salt_);

  scoped_ptr<VaultKeyset> vk(vault_keyset_factory()->New(platform_, crypto_));
  if (!GetValidKeyset(existing_credentials, vk.get())) {
    LOG(WARNING) << "AddKeyset: authentication failed";
    return false;
  }

  // Walk the namespace looking for the first free spot.
  // Optimizations can come later.
  // Note, nothing is stopping simultaenous access to these files
  // or enforcing mandatory locking.
  int new_index = 0;
  FILE* vk_file = NULL;
  std::string vk_path;
  for ( ; new_index < kKeyFileMax; ++new_index) {
    vk_path = GetVaultKeysetPath(obfuscated, new_index);
    // Rely on fopen()'s O_EXCL|O_CREAT behavior to fail
    // repeatedly until there is an opening.
    // TODO(wad) Add a clean-up-0-byte-keysets helper to c-home startup
    vk_file = platform_->OpenFile(vk_path, "wx");
    if (vk_file)  // got one
      break;
  }

  if (!vk_file) {
    LOG(WARNING) << "Failed to find an available keyset slot";
    return false;
  }
  // Once the file has been claimed, we can release the handle.
  platform_->CloseFile(vk_file);

  // Repersist the VK with the new creds.
  bool added = true;
  if (!vk->Encrypt(new_passkey) || !vk->Save(vk_path)) {
    LOG(WARNING) << "Failed to encrypt or write the new keyset";
    added = false;
    platform_->DeleteFile(vk_path, false);
  } else {
    *index = new_index;
  }
  return added;
}

bool HomeDirs::ForceRemoveKeyset(const std::string& obfuscated, int index) {
  // Note, external callers should check credentials.
  if (index < 0 || index >= kKeyFileMax)
    return false;

  std::string path = GetVaultKeysetPath(obfuscated, index);
  if (!platform_->FileExists(path)) {
    LOG(WARNING) << "ForceRemoveKeyset: keyset " << index << " for "
                 << obfuscated << " does not exist";
    // Since it doesn't exist, then we're done.
    return true;
  }
  // TODO(wad) Add file zeroing here or centralize with other code.
  return platform_->DeleteFile(path, false);
}

bool HomeDirs::MoveKeyset(const std::string& obfuscated, int src, int dst) {
  if (src < 0 || dst < 0 || src >= kKeyFileMax || dst >= kKeyFileMax)
    return false;

  std::string src_path = GetVaultKeysetPath(obfuscated, src);
  std::string dst_path = GetVaultKeysetPath(obfuscated, dst);
  if (!platform_->FileExists(src_path))
    return false;
  if (platform_->FileExists(dst_path))
    return false;
  // Grab the destination exclusively
  FILE* vk_file = platform_->OpenFile(dst_path, "wx");
  if (!vk_file)
    return false;
  // The creation occurred so there's no reason to keep the handle.
  platform_->CloseFile(vk_file);
  if (!platform_->Rename(src_path, dst_path))
    return false;
  return true;
}

std::string HomeDirs::GetVaultKeysetPath(const std::string& obfuscated,
                                         int index) const {
  return StringPrintf("%s/%s/%s%d",
                      shadow_root_.c_str(),
                      obfuscated.c_str(),
                      kKeyFile,
                      index);
}

void HomeDirs::RemoveNonOwnerCryptohomesCallback(const FilePath& vault) {
  if (!enterprise_owned_) {  // Enterprise owned? Delete it all.
    std::string owner;
    if (!GetOwner(&owner) ||  // No owner? bail.
        // Don't delete the owner's vault!
        // TODO(wad,ellyjones) Add GetUser*Path-helpers
        vault == FilePath(shadow_root_).Append(owner).Append(kVaultDir))
    return;
  }
  // Once we're sure this is not the owner vault, delete it.
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
  scoped_ptr<FileEnumerator> subdir_enumerator(platform_->GetFileEnumerator(
      dir.value(),
      false,
      static_cast<FileEnumerator::FileType>(
          FileEnumerator::FILES |
          FileEnumerator::DIRECTORIES |
          FileEnumerator::SHOW_SYM_LINKS)));
  for (std::string subdir_path = subdir_enumerator->Next();
       !subdir_path.empty();
       subdir_path = subdir_enumerator->Next()) {
    platform_->DeleteFile(subdir_path, true);
  }
}

void HomeDirs::RemoveNonOwnerDirectories(const FilePath& prefix) {
  std::vector<std::string> dirents;
  if (!platform_->EnumerateDirectoryEntries(prefix.value(), false, &dirents))
    return;
  std::string owner;
  if (!enterprise_owned_ && !GetOwner(&owner))
    return;
  for (std::vector<std::string>::iterator it = dirents.begin();
       it != dirents.end(); ++it) {
    FilePath path(*it);
    const std::string basename = path.BaseName().value();
    if (!enterprise_owned_ && !strcasecmp(basename.c_str(), owner.c_str()))
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
  //  Add a timestamp for every key.
  std::vector<int> key_indices;
  // Failure is okay since the loop falls through.
  GetVaultKeysets(obfuscated_username, &key_indices);
  scoped_ptr<VaultKeyset> keyset(
      vault_keyset_factory()->New(platform_, crypto_));
  std::vector<int>::const_iterator iter = key_indices.begin();
  // Collect the most recent time for a given user by walking all
  // vaults.  This avoids trying to keep them in sync atomically.
  // TODO(wad,?) Move non-key vault metadata to a standalone file.
  base::Time timestamp = base::Time();
  for ( ;iter != key_indices.end(); ++iter) {
    if (LoadVaultKeysetForUser(obfuscated_username, *iter, keyset.get()) &&
        keyset->serialized().has_last_activity_timestamp()) {
      const base::Time t = base::Time::FromInternalValue(
          keyset->serialized().last_activity_timestamp());
      if (t > timestamp)
        timestamp = t;
    }
  }
  if (!timestamp.is_null()) {
      timestamp_cache_->AddExistingUser(user_dir, timestamp);
  } else {
      timestamp_cache_->AddExistingUserNotime(user_dir);
  }
}

bool HomeDirs::LoadVaultKeysetForUser(const std::string& obfuscated_user,
                                      int index,
                                      VaultKeyset* keyset) const {
  // Load the encrypted keyset
  std::string user_key_file = GetVaultKeysetPath(obfuscated_user, index);
  // We don't have keys yet, so just load it.
  // TODO(wad) Move to passing around keysets and not serialized versions.
  if (!keyset->Load(user_key_file)) {
    LOG(ERROR) << "Failed to read keyset file for user " << obfuscated_user;
    return false;
  }
  return true;
}

bool HomeDirs::GetPlainOwner(std::string* owner) {
  if (!policy_provider_->device_policy_is_loaded())
    return false;
  policy_provider_->GetDevicePolicy().GetOwner(owner);
  return true;
}

bool HomeDirs::GetOwner(std::string* owner) {
  std::string plain_owner;
  if (!GetPlainOwner(&plain_owner) || plain_owner.empty())
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
  scoped_refptr<Mount> mount = mount_factory_->New();
  mount->set_platform(platform_);
  mount->set_crypto(crypto_);
  mount->Init();
  std::string obfuscated = newcreds.GetObfuscatedUsername(system_salt_);
  if (!mount->MountCryptohome(oldcreds, Mount::MountArgs(), NULL)) {
    LOG(ERROR) << "Migrate: Mount failed";
    // Fail as early as possible. Note that we don't have to worry about leaking
    // this mount - Mount unmounts itself if it's still mounted in the
    // destructor.
    return false;
  }
  int key_index = mount->CurrentKey();
  if (key_index == -1) {
    LOG(ERROR) << "Attempted migration of key-less mount.";
    return false;
  }
  int new_key_index = -1;
  if (!AddKeyset(oldcreds, newkey, &new_key_index)) {
    LOG(ERROR) << "Failed to add the new keyset";
    return false;
  }

  SecureBlob old_auth_data;
  SecureBlob auth_data;
  std::string username = newcreds.username();
  FilePath salt_file = GetChapsTokenSaltPath(username);
  if (!crypto_->PasskeyToTokenAuthData(newkey, salt_file, &auth_data) ||
      !crypto_->PasskeyToTokenAuthData(oldkey, salt_file, &old_auth_data)) {
    // On failure, drop the newly added keyset so the user can try again.
    ForceRemoveKeyset(obfuscated, new_key_index);
    return false;
  }
  chaps_client_.ChangeTokenAuthData(
      GetChapsTokenDir(username),
      old_auth_data,
      auth_data);

  // Drop the old keyset
  if (!ForceRemoveKeyset(obfuscated, key_index)) {
    LOG(ERROR) << "Migrate: unable to delete the old keyset: " << key_index;
    // TODO(wad) Should we zero it or move it into space?
    // Fallthrough
  }

  // Put the new one in its slot.
  if (!MoveKeyset(obfuscated, new_key_index, key_index)) {
    LOG(ERROR) << "Migrate: failed to move the new key to the old slot";
    // This is bad, but non-terminal since we have a valid, migrated key.
    key_index = new_key_index;
  }

  // Remove all other keysets during a "migration".
  std::vector<int> key_indices;
  if (!GetVaultKeysets(obfuscated, &key_indices)) {
    LOG(WARNING) << "Failed to enumerate keysets after adding one. Weird.";
    // Fallthrough: The user is migrated, but something else changed keys.
  }
  std::vector<int>::const_iterator iter = key_indices.begin();
  for ( ;iter != key_indices.end(); ++iter) {
    if (*iter == key_index)
      continue;
    LOG(INFO) << "Removing keyset " << *iter << " due to migration.";
    ForceRemoveKeyset(obfuscated, *iter);  // Failure is ok.
  }

  return true;
}

namespace {
  const char *kChapsDaemonName = "chaps";
  const char *kChapsDirName = ".chaps";
  const char *kChapsSaltName = "auth_data_salt";
}

FilePath HomeDirs::GetChapsTokenDir(const std::string& user) const {
  return chromeos::cryptohome::home::GetDaemonPath(user, kChapsDaemonName);
}

FilePath HomeDirs::GetLegacyChapsTokenDir(const std::string& user) const {
  return chromeos::cryptohome::home::GetUserPath(user).Append(kChapsDirName);
}

FilePath HomeDirs::GetChapsTokenSaltPath(const std::string& user) const {
  return GetChapsTokenDir(user).Append(kChapsSaltName);
}

}  // namespace cryptohome
