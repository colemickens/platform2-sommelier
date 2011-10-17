// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

#include "mount.h"

#include <errno.h>
#include <sys/stat.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/string_util.h>
#include <base/values.h>
#include <chromeos/cryptohome.h>
#include <chromeos/utility.h>
#include <set>

#include "crypto.h"
#include "cryptohome_common.h"
#include "old_vault_keyset.h"
#include "platform.h"
#include "username_passkey.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

using std::string;

namespace cryptohome {

const char kDefaultUserRoot[] = "/home/chronos";
const char kDefaultHomeDir[] = "/home/chronos/user";
const char kDefaultShadowRoot[] = "/home/.shadow";
const char kDefaultSharedUser[] = "chronos";
const char kDefaultSharedAccessGroup[] = "chronos-access";
const char kDefaultSkeletonSource[] = "/etc/skel";
const char kUserHomeSuffix[] = "user";
const char kRootHomeSuffix[] = "root";
const uid_t kMountOwnerUid = 0;
const gid_t kMountOwnerGid = 0;
// TODO(fes): Remove once UI for BWSI switches to MountGuest()
const char kIncognitoUser[] = "incognito";
// The length of a user's directory name in the shadow root (equal to the length
// of the ascii version of a SHA1 hash)
const unsigned int kUserDirNameLength = 40;
// Encrypted files/directories in ecryptfs have file names that start with the
// following constant.  When clearing tracked subdirectories, we ignore these
// and only delete the pass-through directories.
const char kEncryptedFilePrefix[] = "ECRYPTFS_FNEK_ENCRYPTED.";
// Tracked directories - special sub-directories of the cryptohome
// vault, that are visible even if not mounted. Contents is still encrypted.
const char kCacheDir[] = "Cache";
const char kDownloadsDir[] = "Downloads";
const struct TrackedDir {
  const char* name;
  bool need_migration;
} kTrackedDirs[] = {
  {kCacheDir, false},
  {kDownloadsDir, true},
};
const char kVaultDir[] = "vault";
const base::TimeDelta kOldUserLastActivityTime = base::TimeDelta::FromDays(92);

const char kDefaultEcryptfsCryptoAlg[] = "aes";
const int kDefaultEcryptfsKeySize = CRYPTOHOME_AES_KEY_BYTES;

Mount::Mount()
    : default_user_(-1),
      default_group_(-1),
      default_access_group_(-1),
      default_username_(kDefaultSharedUser),
      home_dir_(kDefaultHomeDir),
      shadow_root_(kDefaultShadowRoot),
      skel_source_(kDefaultSkeletonSource),
      system_salt_(),
      set_vault_ownership_(true),
      default_crypto_(new Crypto()),
      crypto_(default_crypto_.get()),
      default_platform_(new Platform()),
      platform_(default_platform_.get()),
      fallback_to_scrypt_(true),
      use_tpm_(true),
      default_current_user_(new UserSession()),
      current_user_(default_current_user_.get()),
      default_user_timestamp_(new UserOldestActivityTimestampCache()),
      user_timestamp_(default_user_timestamp_.get()),
      enterprise_owned_(false),
      old_user_last_activity_time_(kOldUserLastActivityTime) {
}

Mount::~Mount() {
}

bool Mount::Init() {
  bool result = true;

  // Get the user id and group id of the default user
  if (!platform_->GetUserId(default_username_, &default_user_,
                           &default_group_)) {
    result = false;
  }

  // Get the group id of the default shared access group.
  if (!platform_->GetGroupId(kDefaultSharedAccessGroup,
                             &default_access_group_)) {
    result = false;
  }

  crypto_->set_use_tpm(use_tpm_);
  crypto_->set_fallback_to_scrypt(fallback_to_scrypt_);

  int original_mask = platform_->SetMask(kDefaultUmask);
  // Create the shadow root if it doesn't exist
  FilePath shadow_root(shadow_root_);
  if (!file_util::DirectoryExists(shadow_root)) {
    file_util::CreateDirectory(shadow_root);
  }

  if (!crypto_->Init()) {
    result = false;
  }

  // One-time load of the global system salt (used in generating username
  // hashes)
  FilePath system_salt_file(StringPrintf("%s/salt", shadow_root_.c_str()));
  if (!crypto_->GetOrCreateSalt(system_salt_file,
                                CRYPTOHOME_DEFAULT_SALT_LENGTH, false,
                                &system_salt_)) {
    LOG(ERROR) << "Failed to load or create the system salt";
    result = false;
  }
  platform_->SetMask(original_mask);

  current_user_->Init(crypto_, system_salt_);

  return result;
}

bool Mount::EnsureCryptohome(const Credentials& credentials,
                             const Mount::MountArgs& mount_args,
                             bool* created) const {
  // If the user has an old-style cryptohome, delete it
  FilePath old_image_path(StringPrintf("%s/image",
                                       GetUserDirectory(credentials).c_str()));
  if (file_util::PathExists(old_image_path)) {
    file_util::Delete(FilePath(GetUserDirectory(credentials)), true);
  }
  if (!EnsureUserMountPoints(credentials)) {
    return false;
  }
  // Now check for the presence of a vault directory
  FilePath vault_path(GetUserVaultPath(credentials));
  if (!file_util::DirectoryExists(vault_path)) {
    // If the vault directory doesn't exist, then create the cryptohome from
    // scratch
    bool result = CreateCryptohome(credentials, mount_args);
    if (created) {
      *created = result;
    }
    return result;
  }
  if (created) {
    *created = false;
  }
  return true;
}

bool Mount::DoesCryptohomeExist(const Credentials& credentials) const {
  // Check for the presence of a vault directory
  FilePath vault_path(GetUserVaultPath(credentials));
  return file_util::DirectoryExists(vault_path);
}

bool Mount::MountCryptohome(const Credentials& credentials,
                            const Mount::MountArgs& mount_args,
                            MountError* mount_error) {
  MountError local_mount_error = MOUNT_ERROR_NONE;
  bool result = MountCryptohomeInner(credentials,
                                     mount_args,
                                     true,
                                     &local_mount_error);
  // Retry once if there is a TPM communications failure
  if (!result && local_mount_error == MOUNT_ERROR_TPM_COMM_ERROR) {
    result = MountCryptohomeInner(credentials,
                                  mount_args,
                                  true,
                                  &local_mount_error);
  }
  if (mount_error) {
    *mount_error = local_mount_error;
  }
  return result;
}

bool Mount::MountCryptohomeInner(const Credentials& credentials,
                                 const Mount::MountArgs& mount_args,
                                 bool recreate_decrypt_fatal,
                                 MountError* mount_error) {
  current_user_->Reset();

  std::string username = credentials.GetFullUsernameString();
  if (username.compare(kIncognitoUser) == 0) {
    // TODO(fes): Have guest set error conditions?
    if (mount_error) {
      *mount_error = MOUNT_ERROR_NONE;
    }
    return MountGuestCryptohome();
  }

  if (!mount_args.create_if_missing && !DoesCryptohomeExist(credentials)) {
    if (mount_error) {
      *mount_error = MOUNT_ERROR_USER_DOES_NOT_EXIST;
    }
    return false;
  }

  bool created = false;
  if (!EnsureCryptohome(credentials, mount_args, &created)) {
    LOG(ERROR) << "Error creating cryptohome.";
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // Attempt to decrypt the vault keyset with the specified credentials
  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  MountError local_mount_error = MOUNT_ERROR_NONE;
  if (!DecryptVaultKeyset(credentials, true, &vault_keyset, &serialized,
                         &local_mount_error)) {
    if (mount_error) {
      *mount_error = local_mount_error;
    }
    if (recreate_decrypt_fatal & (local_mount_error & MOUNT_ERROR_FATAL)) {
      LOG(ERROR) << "Error, cryptohome must be re-created because of fatal "
                 << "error.";
      if (!RemoveCryptohome(credentials)) {
        LOG(ERROR) << "Fatal decryption error, but unable to remove "
                   << "cryptohome.";
        return false;
      }
      // Allow one recursion into MountCryptohomeInner by blocking re-create on
      // fatal.
      bool local_result = MountCryptohomeInner(credentials,
                                               mount_args,
                                               false,
                                               mount_error);
      // If the mount was successful, set the status to indicate that the
      // cryptohome was recreated.
      if (local_result && mount_error) {
        *mount_error = MOUNT_ERROR_RECREATED;
      }
      return local_result;
    }
    return false;
  }
  crypto_->ClearKeyset();

  // Add the decrypted key to the keyring so that ecryptfs can use it
  string key_signature, fnek_signature;
  if (!crypto_->AddKeyset(vault_keyset, &key_signature, &fnek_signature)) {
    LOG(INFO) << "Cryptohome mount failed because of keyring failure.";
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // Specify the ecryptfs options for mounting the user's cryptohome
  string ecryptfs_options = StringPrintf("ecryptfs_cipher=aes"
                                         ",ecryptfs_key_bytes=%d"
                                         ",ecryptfs_fnek_sig=%s"
                                         ",ecryptfs_sig=%s"
                                         ",ecryptfs_unlink_sigs",
                                         kDefaultEcryptfsKeySize,
                                         fnek_signature.c_str(),
                                         key_signature.c_str());

  // Mount cryptohome
  // /home/.shadow: owned by root
  // /home/.shadow/$hash: owned by root
  // /home/.shadow/$hash/vault: owned by root
  // /home/.shadow/$hash/mount: owned by root
  // /home/.shadow/$hash/mount/root: owned by root
  // /home/.shadow/$hash/mount/user: owned by chronos
  // /home/chronos: owned by chronos
  // /home/chronos/user: owned by chronos
  // /home/user/$hash: owned by chronos
  // /home/root/$hash: owned by root

  string vault_path = GetUserVaultPath(credentials);
  // Create vault_path/user as a passthrough directory, move all the (encrypted)
  // contents of vault_path into vault_path/user, create vault_path/root.
  MigrateToUserHome(vault_path);

  mount_point_ = GetUserMountDirectory(credentials);
  if (!platform_->CreateDirectory(mount_point_)) {
    PLOG(ERROR) << "Directory creation failed for " << mount_point_;
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  if (!platform_->Mount(vault_path, mount_point_, "ecryptfs", ecryptfs_options)) {
   PLOG(ERROR) << "Cryptohome mount failed for vault " << vault_path;
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // Move the tracked subdirectories from mount_point_/user to vault_path
  // as passthrough directories.
  CreateTrackedSubdirectories(credentials, created);

  string user_home = GetMountedUserHomePath(credentials);
  if (!platform_->Bind(user_home, home_dir_)) {
    PLOG(ERROR) << "Bind mount failed: " << user_home << " -> " << home_dir_;
    if (mount_error)
      *mount_error = MOUNT_ERROR_FATAL;
    platform_->Unmount(mount_point_, false, NULL);
    return false;
  }

  // TODO(ellyjones): Expose the path to the root directory over dbus for use by
  // daemons. We may also want to bind-mount it somewhere stable.

  if (created) {
    CopySkeletonForUser(credentials);
  }

  if (!SetupGroupAccess()) {
    if (mount_error)
      *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }

  if (mount_error) {
    *mount_error = MOUNT_ERROR_NONE;
  }

  current_user_->SetUser(credentials);
  return true;
}

bool Mount::UnmountCryptohome() {
  UpdateCurrentUserActivityTimestamp(0);
  current_user_->Reset();

  // Try an immediate unmount
  bool was_busy;
  if (!platform_->Unmount(home_dir_, false, &was_busy)) {
    LOG(ERROR) << "Couldn't unmount vault immediately, was_busy = " << was_busy;
    if (was_busy) {
      std::vector<ProcessInformation> processes;
      platform_->GetProcessesWithOpenFiles(home_dir_, &processes);
      for (std::vector<ProcessInformation>::iterator proc_itr =
             processes.begin();
           proc_itr != processes.end();
           proc_itr++) {
        LOG(ERROR) << "Process " << proc_itr->get_process_id()
                   << " had open files.  Command line: "
                   << proc_itr->GetCommandLine();
        if (proc_itr->get_cwd().length()) {
          LOG(ERROR) << "  (" << proc_itr->get_process_id() << ") CWD: "
                     << proc_itr->get_cwd();
        }
        for (std::set<std::string>::iterator file_itr =
               proc_itr->get_open_files().begin();
             file_itr != proc_itr->get_open_files().end();
             file_itr++) {
          LOG(ERROR) << "  (" << proc_itr->get_process_id() << ") Open File: "
                     << (*file_itr);
        }
      }
      sync();
    }
    // Failed to unmount immediately, do a lazy unmount
    platform_->Unmount(home_dir_, true, NULL);
    sync();
  }

  platform_->Unmount(mount_point_, true, NULL);

  // Clear the user keyring if the unmount was successful
  crypto_->ClearKeyset();

  return true;
}

bool Mount::RemoveCryptohome(const Credentials& credentials) {
  std::string user_dir = GetUserDirectory(credentials);
  CHECK(user_dir.length() > (shadow_root_.length() + 1));

  if (IsCryptohomeMountedForUser(credentials)) {
    if (!UnmountCryptohome()) {
      return false;
    }
  }

  return file_util::Delete(FilePath(user_dir), true);
}

bool Mount::IsCryptohomeMounted() const {
  return platform_->IsDirectoryMounted(home_dir_);
}

bool Mount::IsCryptohomeMountedForUser(const Credentials& credentials) const {
  return platform_->IsDirectoryMountedWith(home_dir_,
                                           GetUserVaultPath(credentials));
}

bool Mount::CreateCryptohome(const Credentials& credentials,
                             const Mount::MountArgs& mount_args) const {
  int original_mask = platform_->SetMask(kDefaultUmask);

  // Create the user's entry in the shadow root
  FilePath user_dir(GetUserDirectory(credentials));
  file_util::CreateDirectory(user_dir);

  // Generate a new master key
  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(*this);
  SerializedVaultKeyset serialized;
  if (!AddVaultKeyset(credentials, vault_keyset, &serialized)) {
    platform_->SetMask(original_mask);
    return false;
  }
  if (!StoreVaultKeyset(credentials, serialized)) {
    platform_->SetMask(original_mask);
    return false;
  }

  // Create the user's vault.
  std::string vault_path = GetUserVaultPath(credentials);
  if (!platform_->CreateDirectory(vault_path)) {
    LOG(ERROR) << "Couldn't create vault path: " << vault_path.c_str();
    platform_->SetMask(original_mask);
    return false;
  }

  // Restore the umask
  platform_->SetMask(original_mask);
  return true;
}

bool Mount::CreateTrackedSubdirectories(const Credentials& credentials,
                                        bool is_new) const {
  const int original_mask = platform_->SetMask(kDefaultUmask);

  // Add the subdirectories if they do not exist.
  const FilePath dest = FilePath(VaultPathToUserPath(GetUserVaultPath(credentials)));
  const FilePath source = FilePath(GetMountedUserHomePath(credentials));
  if (!file_util::DirectoryExists(dest)) {
    LOG(ERROR) << "Can't create tracked subdirectories for a missing user.";
    platform_->SetMask(original_mask);
    return false;
  }

  // The call is allowed to partially fail if directory creation fails, but we
  // want to have as many of the specified tracked directories created as
  // possible.
  bool result = true;
  for (size_t index = 0; index < arraysize(kTrackedDirs); ++index) {
    const TrackedDir& subdir = kTrackedDirs[index];
    const string subdir_name = subdir.name;
    const FilePath passthrough_dir = dest.Append(subdir_name);
    const FilePath old_dir = source.Append(subdir_name);

    // Start migrating if |subdir| is not a pass-through directory yet.
    FilePath tmp_migrated_dir;
    if (!is_new && file_util::DirectoryExists(old_dir) &&
        !file_util::DirectoryExists(passthrough_dir)) {
      if (!subdir.need_migration) {
        LOG(INFO) << "Removing non-pass-through " << old_dir.value() << ". "
                  << "Migration not requested.";
        file_util::Delete(old_dir, true);
      } else {
        // Migrate it: rename it, and after the pass-through one is
        // created, move its contents there.
        tmp_migrated_dir = FilePath(dest).Append(subdir_name + ".tmp");
        LOG(INFO) << "Moving migration directory " << old_dir.value()
                  << " to " << tmp_migrated_dir.value() << "...";
        if (!file_util::Move(old_dir, tmp_migrated_dir)) {
          LOG(ERROR) << "Moving migration directory " << old_dir.value()
                     << " to " << tmp_migrated_dir.value() << " failed. "
                     << "Deleting it.";
          file_util::Delete(old_dir, true);
          tmp_migrated_dir.clear();
          result = false;
        }
      }
    }

    // Create pass-through directory.
    if (!file_util::DirectoryExists(passthrough_dir)) {
      LOG(INFO) << "create-tracked: " << passthrough_dir.value();
      file_util::CreateDirectory(passthrough_dir);
      if (set_vault_ownership_) {
        if (!platform_->SetOwnership(passthrough_dir.value(),
                                     default_user_, default_group_)) {
          LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                     << default_group_ << ") of tracked directory path: "
                     << passthrough_dir.value();
          file_util::Delete(passthrough_dir, true);
          result = false;
          continue;
        }
      }
    }

    // Finish migration if started for this directory.
    if (!tmp_migrated_dir.empty()) {
      const FilePath new_dir = FilePath(dest).Append(subdir_name);
      if (!file_util::DirectoryExists(new_dir)) {
        LOG(ERROR) << "Unable to locate created pass-through directory from "
                   << new_dir.value() << ". Are we in a unit-test?";

        // For the test sake (where we don't have real mount), just create it.
        file_util::CreateDirectory(new_dir);
      }
      LOG(INFO) << "Moving migration directory " << tmp_migrated_dir.value()
                << " to " << new_dir.value() << "...";
      if (!file_util::Move(tmp_migrated_dir, new_dir)) {
        LOG(ERROR) << "Unable to move.";
        result = false;
      }
      file_util::Delete(tmp_migrated_dir, true);
    }
  }

  // Restore the umask
  platform_->SetMask(original_mask);
  return result;
}

bool Mount::UpdateCurrentUserActivityTimestamp(int time_shift_sec) {
  string obfuscated_username;
  current_user_->GetObfuscatedUsername(&obfuscated_username);
  if (!obfuscated_username.empty()) {
    SerializedVaultKeyset serialized;
    LoadVaultKeysetForUser(obfuscated_username, &serialized);
    base::Time timestamp = platform_->GetCurrentTime();
    if (time_shift_sec > 0)
      timestamp -= base::TimeDelta::FromSeconds(time_shift_sec);
    serialized.set_last_activity_timestamp(timestamp.ToInternalValue());
    StoreVaultKeysetForUser(obfuscated_username, serialized);
    if (user_timestamp_->initialized()) {
      user_timestamp_->UpdateExistingUser(
          FilePath(GetUserDirectoryForUser(obfuscated_username)), timestamp);
    }
    return true;
  }
  return false;
}

void Mount::DoForEveryUnmountedCryptohome(CryptohomeCallback callback) {
  FilePath shadow_root(shadow_root_);
  file_util::FileEnumerator dir_enumerator(shadow_root, false,
      file_util::FileEnumerator::DIRECTORIES);
  for (FilePath next_path = dir_enumerator.Next(); !next_path.empty();
       next_path = dir_enumerator.Next()) {
    FilePath dir_name = next_path.BaseName();
    string str_dir_name = dir_name.value();
    if (str_dir_name.length() != kUserDirNameLength) {
      continue;
    }
    bool valid_name = true;
    for (string::const_iterator itr = str_dir_name.begin();
          itr < str_dir_name.end(); ++itr) {
      if (!isxdigit(*itr)) {
        valid_name = false;
        break;
      }
    }
    if (!valid_name) {
      continue;
    }
    const FilePath vault_path = next_path.Append(kVaultDir);
    if (!file_util::DirectoryExists(vault_path)) {
      continue;
    }
    if (platform_->IsDirectoryMountedWith(home_dir_, vault_path.value())) {
      continue;
    }
    (this->*callback)(vault_path);
  }
}

void Mount::DeleteTrackedDirsCallback(const FilePath& vault) {
  file_util::FileEnumerator subdir_enumerator(
      vault, false, file_util::FileEnumerator::DIRECTORIES);
  for (FilePath subdir_path = subdir_enumerator.Next(); !subdir_path.empty();
       subdir_path = subdir_enumerator.Next()) {
    FilePath subdir_name = subdir_path.BaseName();
    if (subdir_name.value().find(kEncryptedFilePrefix) == 0) {
      continue;
    }
    if (subdir_name.value().compare(".") == 0 ||
        subdir_name.value().compare("..") == 0) {
      continue;
    }
    file_util::Delete(subdir_path, true);
  }
}

// Deletes specified directory contents, but leaves the directory itself.
static void DeleteDirectoryContents(const FilePath& dir) {
  file_util::FileEnumerator subdir_enumerator(
      dir,
      false,
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::SHOW_SYM_LINKS));
  for (FilePath subdir_path = subdir_enumerator.Next(); !subdir_path.empty();
       subdir_path = subdir_enumerator.Next()) {
    file_util::Delete(subdir_path, true);
  }
}

void Mount::CleanUnmountedTrackedSubdirectories() {
  DoForEveryUnmountedCryptohome(&Mount::DeleteTrackedDirsCallback);
}

void Mount::DeleteCacheCallback(const FilePath& vault) {
  LOG(WARNING) << "Deleting Cache for user " << vault.value();
  DeleteDirectoryContents(vault.Append(kCacheDir));
}

void Mount::AddUserTimestampToCacheCallback(const FilePath& vault) {
  const FilePath user_dir = vault.DirName();
  const std::string obfuscated_username = user_dir.BaseName().value();
  SerializedVaultKeyset serialized;
  LoadVaultKeysetForUser(obfuscated_username, &serialized);
  if (serialized.has_last_activity_timestamp()) {
    const base::Time timestamp = base::Time::FromInternalValue(
        serialized.last_activity_timestamp());
    user_timestamp_->AddExistingUser(user_dir, timestamp);
  } else {
    user_timestamp_->AddExistingUserNotime(user_dir);
  }
}

bool Mount::DoAutomaticFreeDiskSpaceControl() {
  if (platform_->AmountOfFreeDiskSpace(home_dir_) > kMinFreeSpace)
    return false;

  // Clean Cache directories for every user (except current one).
  DoForEveryUnmountedCryptohome(&Mount::DeleteCacheCallback);

  if (platform_->AmountOfFreeDiskSpace(home_dir_) >= kEnoughFreeSpace)
    return true;

  // Initialize user timestamp cache if it has not been yet.  Current
  // user is not added now, but added on log out or during daily
  // updates (UpdateCurrentUserActivityTimestamp()).
  if (!user_timestamp_->initialized()) {
    user_timestamp_->Initialize();
    DoForEveryUnmountedCryptohome(&Mount::AddUserTimestampToCacheCallback);
  }

  // Delete old users, the oldest first.
  // Don't delete anyone if we don't know who the owner is.
  if (enterprise_owned_ || !owner_obfuscated_username_.empty()) {
    const base::Time timestamp_threshold =
        base::Time::Now() - old_user_last_activity_time_;
    while (!user_timestamp_->oldest_known_timestamp().is_null() &&
           user_timestamp_->oldest_known_timestamp() <= timestamp_threshold) {
      FilePath deleted_user_dir = user_timestamp_->RemoveOldestUser();
      if (!enterprise_owned_) {
        std::string obfuscated_username = deleted_user_dir.BaseName().value();
        if (obfuscated_username == owner_obfuscated_username_) {
          LOG(WARNING) << "Not deleting owner user " << obfuscated_username;
          continue;
        }
      }
      if (platform_->IsDirectoryMountedWith(
              home_dir_, deleted_user_dir.Append(kVaultDir).value())) {
        LOG(WARNING) << "Attempt to delete currently logged user. Skipped...";
      } else {
        LOG(WARNING) << "Deleting old user " << deleted_user_dir.value();
        platform_->DeleteFile(deleted_user_dir.value(), true);
        if (platform_->AmountOfFreeDiskSpace(home_dir_) >= kEnoughFreeSpace)
          return true;
      }
    }
  }

  // TODO(glotov): do further cleanup.
  return true;
}

void Mount::SetOwnerUser(const string& username) {
  if (!username.empty()) {
    owner_obfuscated_username_ = UsernamePasskey(
        username.c_str(), chromeos::Blob()).GetObfuscatedUsername(system_salt_);
  }
}

bool Mount::SetupGroupAccess() const {
  if (!set_vault_ownership_)
    return true;

  // Make the following directories group accessible by other system daemons:
  //   /home/chronos/user
  //   /home/chronos/user/Downloads
  FilePath downloads_path = FilePath(home_dir_).Append(kDownloadsDir);
  mode_t mode = S_IXGRP;
  if (!platform_->SetGroupAccessible(home_dir_, default_access_group_, mode) ||
      !platform_->SetGroupAccessible(downloads_path.value(),
                                     default_access_group_, mode))
    return false;

  return true;
}

bool Mount::TestCredentials(const Credentials& credentials) const {
  // If the current logged in user matches, use the UserSession to verify the
  // credentials.  This is less costly than a trip to the TPM, and only verifies
  // a user during their logged in session.
  if (current_user_->CheckUser(credentials)) {
    return current_user_->Verify(credentials);
  }
  MountError mount_error;
  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  bool result = DecryptVaultKeyset(credentials, false, &vault_keyset,
                                   &serialized, &mount_error);
  // Retry once if there is a TPM communications failure
  if (!result && mount_error == MOUNT_ERROR_TPM_COMM_ERROR) {
    result = DecryptVaultKeyset(credentials, false, &vault_keyset,
                                &serialized, &mount_error);
  }
  return result;
}

bool Mount::LoadVaultKeyset(const Credentials& credentials,
                            SerializedVaultKeyset* serialized) const {
  return LoadVaultKeysetForUser(credentials.GetObfuscatedUsername(system_salt_),
                                serialized);
}

bool Mount::LoadVaultKeysetForUser(const string& obfuscated_username,
                                   SerializedVaultKeyset* serialized) const {
  // Load the encrypted keyset
  FilePath user_key_file(GetUserKeyFileForUser(obfuscated_username));
  if (!file_util::PathExists(user_key_file)) {
    return false;
  }
  SecureBlob cipher_text;
  if (!LoadFileBytes(user_key_file, &cipher_text)) {
    return false;
  }
  if (!serialized->ParseFromArray(
           static_cast<const unsigned char*>(cipher_text.data()),
           cipher_text.size())) {
    return false;
  }
  return true;
}

bool Mount::StoreVaultKeyset(const Credentials& credentials,
                             const SerializedVaultKeyset& serialized) const {
  return StoreVaultKeysetForUser(
      credentials.GetObfuscatedUsername(system_salt_),
      serialized);
}

bool Mount::StoreVaultKeysetForUser(
    const string& obfuscated_username,
    const SerializedVaultKeyset& serialized) const {
  SecureBlob final_blob(serialized.ByteSize());
  serialized.SerializeWithCachedSizesToArray(
      static_cast<google::protobuf::uint8*>(final_blob.data()));
  unsigned int data_written = file_util::WriteFile(
      FilePath(GetUserKeyFileForUser(obfuscated_username)),
      static_cast<const char*>(final_blob.const_data()),
      final_blob.size());

  if (data_written != final_blob.size()) {
    return false;
  }
  return true;
}

bool Mount::DecryptVaultKeyset(const Credentials& credentials,
                               bool migrate_if_needed,
                               VaultKeyset* vault_keyset,
                               SerializedVaultKeyset* serialized,
                               MountError* error) const {
  FilePath salt_path(GetUserSaltFile(credentials));
  // If a separate salt file exists, it is the old-style keyset
  if (file_util::PathExists(salt_path)) {
    VaultKeyset old_keyset;
    if (!DecryptVaultKeysetOld(credentials, &old_keyset, error)) {
      return false;
    }
    if (migrate_if_needed) {
      // This is not considered a fatal error.  Re-saving with the desired
      // protection is ideal, but not required.
      ReEncryptVaultKeyset(credentials, old_keyset, serialized);
    }
    vault_keyset->FromVaultKeyset(old_keyset);
    return true;
  } else {
    SecureBlob passkey;
    credentials.GetPasskey(&passkey);

    // Load the encrypted keyset
    if (!LoadVaultKeyset(credentials, serialized)) {
      if (error) {
        *error = MOUNT_ERROR_FATAL;
      }
      return false;
    }

    // Attempt decrypt the master key with the passkey
    unsigned int crypt_flags = 0;
    Crypto::CryptoError crypto_error = Crypto::CE_NONE;
    if (!crypto_->DecryptVaultKeyset(*serialized, passkey, &crypt_flags,
                                     &crypto_error, vault_keyset)) {
      if (error) {
        switch (crypto_error) {
          case Crypto::CE_TPM_FATAL:
          case Crypto::CE_OTHER_FATAL:
            *error = Mount::MOUNT_ERROR_FATAL;
            break;
          case Crypto::CE_TPM_COMM_ERROR:
            *error = Mount::MOUNT_ERROR_TPM_COMM_ERROR;
            break;
          case Crypto::CE_TPM_DEFEND_LOCK:
            *error = Mount::MOUNT_ERROR_TPM_DEFEND_LOCK;
            break;
          default:
            *error = Mount::MOUNT_ERROR_KEY_FAILURE;
            break;
        }
      }
      return false;
    }

    if (migrate_if_needed) {
      // Calling EnsureTpm here handles the case where a user logged in while
      // cryptohome was taking TPM ownership.  In that case, their vault keyset
      // would be scrypt-wrapped and the TPM would not be connected.  If we're
      // configured to use the TPM, calling EnsureTpm will try to connect, and
      // if successful, the call to has_tpm() below will succeed, allowing
      // re-wrapping (migration) using the TPM.
      if (use_tpm_) {
        crypto_->EnsureTpm(false);
      }

      // If the vault keyset's TPM state is not the same as that configured for
      // the device, re-save the keyset (this will save in the device's default
      // method).
      //                      1   2   3   4   5   6   7   8   9  10  11  12
      // use_tpm              -   -   -   X   X   X   X   X   X   -   -   -
      //
      // fallback_to_scrypt   -   -   -   -   -   -   X   X   X   X   X   X
      //
      // tpm_wrapped          -   X   -   -   X   -   -   X   -   -   X   -
      //
      // scrypt_wrapped       -   -   X   -   -   X   -   -   X   -   -   X
      //
      // migrate              N   Y   Y   Y   N   Y   Y   N   Y   Y   Y   N
      bool tpm_wrapped =
          (crypt_flags & SerializedVaultKeyset::TPM_WRAPPED) != 0;
      bool scrypt_wrapped =
          (crypt_flags & SerializedVaultKeyset::SCRYPT_WRAPPED) != 0;
      bool should_tpm = (crypto_->has_tpm() && use_tpm_ &&
                         crypto_->is_tpm_connected());
      bool should_scrypt = fallback_to_scrypt_;
      do {
        // If the keyset was TPM-wrapped, but there was no public key hash,
        // always re-save.  Otherwise, check the table.
        if (crypto_error != Crypto::CE_NO_PUBLIC_KEY_HASH) {
          if (tpm_wrapped && should_tpm)
            break; // 5, 8
          if (scrypt_wrapped && should_scrypt && !should_tpm)
            break; // 12
          if (!tpm_wrapped && !scrypt_wrapped && !should_tpm && !should_scrypt)
            break; // 1
        }
        // This is not considered a fatal error.  Re-saving with the desired
        // protection is ideal, but not required.
        SerializedVaultKeyset new_serialized;
        new_serialized.CopyFrom(*serialized);
        if (ReEncryptVaultKeyset(credentials, *vault_keyset, &new_serialized)) {
          serialized->CopyFrom(new_serialized);
        }
      } while(false);
    }

    return true;
  }
}

bool Mount::AddVaultKeyset(const Credentials& credentials,
                           const VaultKeyset& vault_keyset,
                           SerializedVaultKeyset* serialized) const {
  // We don't do passkey to wrapper conversion because it is salted during save
  SecureBlob passkey;
  credentials.GetPasskey(&passkey);

  // Encrypt the vault keyset
  SecureBlob salt(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  crypto_->GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                           salt.size());

  if (!crypto_->EncryptVaultKeyset(vault_keyset, passkey, salt, serialized)) {
    LOG(ERROR) << "Encrypting vault keyset failed";
    return false;
  }

  return true;
}

bool Mount::ReEncryptVaultKeyset(const Credentials& credentials,
                                 const VaultKeyset& vault_keyset,
                              SerializedVaultKeyset* serialized) const {
  std::vector<std::string> files(2);
  files[0] = GetUserSaltFile(credentials);
  files[1] = GetUserKeyFile(credentials);
  if (!CacheOldFiles(credentials, files)) {
    LOG(ERROR) << "Couldn't cache old key material.";
    return false;
  }
  if (!AddVaultKeyset(credentials, vault_keyset, serialized)) {
    LOG(ERROR) << "Couldn't add keyset.";
    RevertCacheFiles(credentials, files);
    return false;
  }
  if (!StoreVaultKeyset(credentials, *serialized)) {
    LOG(ERROR) << "Write to master key failed";
    RevertCacheFiles(credentials, files);
    return false;
  }
  DeleteCacheFiles(credentials, files);
  return true;
}

bool Mount::MigratePasskey(const Credentials& credentials,
                           const char* old_key) const {

  std::string username = credentials.GetFullUsernameString();
  UsernamePasskey old_credentials(username.c_str(),
                                  SecureBlob(old_key, strlen(old_key)));
  // Attempt to decrypt the vault keyset with the specified credentials
  MountError mount_error;
  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  bool result = DecryptVaultKeyset(old_credentials, false, &vault_keyset,
                                   &serialized, &mount_error);
  // Retry once if there is a TPM communications failure
  if (!result && mount_error == MOUNT_ERROR_TPM_COMM_ERROR) {
    result = DecryptVaultKeyset(old_credentials, false, &vault_keyset,
                                &serialized, &mount_error);
  }
  if (result) {
    if (!ReEncryptVaultKeyset(credentials, vault_keyset, &serialized)) {
      LOG(ERROR) << "Couldn't save keyset.";
      current_user_->Reset();
      return false;
    }
    current_user_->SetUser(credentials);
    return true;
  }
  current_user_->Reset();
  return false;
}

bool Mount::MountGuestCryptohome() const {
  current_user_->Reset();

  // Attempt to mount guestfs
  if (!platform_->Mount("guestfs", home_dir_, "tmpfs", "mode=0700")) {
    LOG(ERROR) << "Cryptohome mount failed: " << errno << " for guestfs";
    return false;
  }
  if (set_vault_ownership_) {
    if (!platform_->SetOwnership(home_dir_, default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of guestfs path: "
                 << home_dir_;
      platform_->Unmount(home_dir_,
                         false,  // not lazy
                         NULL);  // ignore was_busy
      return false;
    }
  }
  CopySkeleton();

  // Create the Downloads directory if it does not exist so that it can
  // later be made group accessible when SetupGroupAccess() is called.
  FilePath downloads_path = FilePath(home_dir_).Append(kDownloadsDir);
  if (!file_util::DirectoryExists(downloads_path)) {
    if (!file_util::CreateDirectory(downloads_path) ||
        !platform_->SetOwnership(downloads_path.value(),
                                 default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't create user Downloads directory: "
                 << downloads_path.value();
      platform_->Unmount(home_dir_,
                         false,  // not lazy
                         NULL);  // ignore was_busy
      return false;
    }
  }

  if (!SetupGroupAccess()) {
    platform_->Unmount(home_dir_, false, NULL);
    return false;
  }

  return true;
}

string Mount::GetUserDirectory(const Credentials& credentials) const {
  return GetUserDirectoryForUser(
      credentials.GetObfuscatedUsername(system_salt_));
}

string Mount::GetUserDirectoryForUser(const string& obfuscated_username) const {
  return StringPrintf("%s/%s",
                      shadow_root_.c_str(),
                      obfuscated_username.c_str());
}

string Mount::GetUserSaltFile(const Credentials& credentials) const {
  return StringPrintf("%s/%s/master.0.salt",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str());
}

string Mount::GetUserKeyFile(const Credentials& credentials) const {
  return GetUserKeyFileForUser(credentials.GetObfuscatedUsername(system_salt_));
}

string Mount::GetUserKeyFileForUser(const string& obfuscated_username) const {
  return StringPrintf("%s/%s/master.0",
                      shadow_root_.c_str(),
                      obfuscated_username.c_str());
}

string Mount::GetUserVaultPath(const Credentials& credentials) const {
  return StringPrintf("%s/%s/%s",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str(),
                      kVaultDir);
}

string Mount::GetUserMountDirectory(const Credentials& credentials) const {
  return StringPrintf("%s/%s/mount",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str());
}

string Mount::GetMountedUserHomePath(const Credentials& credentials) const {
  return StringPrintf("%s/%s", GetUserMountDirectory(credentials).c_str(),
                      kUserHomeSuffix);
}

string Mount::VaultPathToUserPath(const std::string& vault) const {
  return StringPrintf("%s/%s", vault.c_str(), kUserHomeSuffix);
}

string Mount::VaultPathToRootPath(const std::string& vault) const {
  return StringPrintf("%s/%s", vault.c_str(), kRootHomeSuffix);
}

void Mount::RecursiveCopy(const FilePath& destination,
                          const FilePath& source) const {
  file_util::FileEnumerator file_enumerator(source, false,
                                            file_util::FileEnumerator::FILES);
  FilePath next_path;
  while (!(next_path = file_enumerator.Next()).empty()) {
    FilePath file_name = next_path.BaseName();
    FilePath destination_file = destination.Append(file_name);
    file_util::CopyFile(next_path, destination_file);
    if (set_vault_ownership_) {
      if (chown(destination_file.value().c_str(), default_user_,
               default_group_)) {
        LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                   << default_group_ << ") of skeleton path: "
                   << destination_file.value().c_str();
      }
    }
  }
  file_util::FileEnumerator dir_enumerator(source, false,
      file_util::FileEnumerator::DIRECTORIES);
  while (!(next_path = dir_enumerator.Next()).empty()) {
    FilePath dir_name = next_path.BaseName();
    FilePath destination_dir = destination.Append(dir_name);
    LOG(INFO) << "RecursiveCopy: " << destination_dir.value();
    file_util::CreateDirectory(destination_dir);
    if (set_vault_ownership_) {
      if (chown(destination_dir.value().c_str(), default_user_,
               default_group_)) {
        LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                   << default_group_ << ") of skeleton path: "
                   << destination_dir.value().c_str();
      }
    }
    RecursiveCopy(destination_dir, next_path);
  }
}

void Mount::MigrateToUserHome(const std::string& vault_path) const
{
  std::vector<string> ent_list;
  std::vector<string>::iterator ent_iter;
  FilePath user_path(VaultPathToUserPath(vault_path));
  FilePath root_path(VaultPathToRootPath(vault_path));
  struct stat st;

  // This check makes the migration idempotent; if we completed a migration,
  // root_path will exist and we're done, and if we didn't complete it, we can
  // finish it.
  if (platform_->Stat(root_path.value(), &st) &&
      S_ISDIR(st.st_mode) &&
      st.st_uid == kMountOwnerUid &&
      st.st_gid == kMountOwnerGid) {
      return;
  }

  // There are three ways to get here:
  // 1) the Stat() call above succeeded, but what we saw was not a root-owned
  //    directory.
  // 2) the Stat() call above failed with -ENOENT
  // 3) the Stat() call above failed for some other reason
  // In any of these cases, it is safe for us to rm root_path, since the only
  // way it could have gotten there is if someone undertook some funny business
  // as root.
  platform_->DeleteFile(root_path.value(), true);

  // Get the list of entries before we create user_path, since user_path will be
  // inside dir.
  platform_->EnumerateDirectoryEntries(vault_path, false, &ent_list);

  if (!platform_->CreateDirectory(user_path.value())) {
    PLOG(ERROR) << "CreateDirectory() failed: " << user_path.value();
    return;
  }

  if (!platform_->SetOwnership(user_path.value(), default_user_, default_group_)) {
    PLOG(ERROR) << "SetOwnership() failed: " << user_path.value();
    return;
  }

  for (ent_iter = ent_list.begin(); ent_iter != ent_list.end(); ent_iter++) {
    FilePath basename(*ent_iter);
    FilePath next_path = basename;
    basename = basename.BaseName();
    // Don't move the user/ directory itself. We're currently operating on an
    // _unmounted_ ecryptfs, which means all the filenames are encrypted except
    // the user and root passthrough directories.
    if (basename.value() == kUserHomeSuffix) {
      LOG(WARNING) << "Interrupted migration detected.";
      continue;
    }
    FilePath dest_path(user_path);
    dest_path = dest_path.Append(basename);
    if (!platform_->Rename(next_path.value(), dest_path.value())) {
      // TODO(ellyjones): UMA event log for this
      PLOG(WARNING) << "Migration fault: can't move " << next_path.value()
                    << " to " << dest_path.value();
    }
  }
  // Create root_path at the end as a sentinel for migration.
  if (!platform_->CreateDirectory(root_path.value())) {
    PLOG(ERROR) << "CreateDirectory() failed: " << root_path.value();
    return;
  }
  LOG(INFO) << "Migrated user directory: " << vault_path.c_str();
}

void Mount::CopySkeletonForUser(const Credentials& credentials) const {
  if (IsCryptohomeMountedForUser(credentials)) {
    CopySkeleton();
  }
}

void Mount::CopySkeleton() const {
  RecursiveCopy(FilePath(home_dir_), FilePath(skel_source_));
}

void Mount::GetSecureRandom(unsigned char *rand, unsigned int length) const {
  crypto_->GetSecureRandom(rand, length);
}

bool Mount::RemoveOldFiles(const Credentials& credentials) const {
  FilePath key_file(GetUserKeyFile(credentials));
  if (file_util::PathExists(key_file)) {
    if (!file_util::Delete(key_file, false)) {
      return false;
    }
  }
  FilePath salt_file(GetUserSaltFile(credentials));
  if (file_util::PathExists(salt_file)) {
    if (!file_util::Delete(salt_file, false)) {
      return false;
    }
  }
  return true;
}

bool Mount::CacheOldFiles(const Credentials& credentials,
                          std::vector<std::string>& files) const {
  for (unsigned int index = 0; index < files.size(); ++index) {
    FilePath file(files[index]);
    FilePath file_bak(StringPrintf("%s.bak", files[index].c_str()));
    if (file_util::PathExists(file_bak)) {
      if (!file_util::Delete(file_bak, false)) {
        return false;
      }
    }
    if (file_util::PathExists(file)) {
      if (!file_util::Move(file, file_bak)) {
        return false;
      }
    }
  }
  return true;
}

bool Mount::RevertCacheFiles(const Credentials& credentials,
                             std::vector<std::string>& files) const {
  for (unsigned int index = 0; index < files.size(); ++index) {
    FilePath file(files[index]);
    FilePath file_bak(StringPrintf("%s.bak", files[index].c_str()));
    if (file_util::PathExists(file_bak)) {
      if (!file_util::Move(file_bak, file)) {
        return false;
      }
    }
  }
  return true;
}

bool Mount::DeleteCacheFiles(const Credentials& credentials,
                             std::vector<std::string>& files) const {
  for (unsigned int index = 0; index < files.size(); ++index) {
    FilePath file(files[index]);
    FilePath file_bak(StringPrintf("%s.bak", files[index].c_str()));
    if (file_util::PathExists(file_bak)) {
      if (!file_util::Delete(file_bak, false)) {
        return false;
      }
    }
  }
  return true;
}

void Mount::GetSystemSalt(chromeos::Blob* salt) const {
  *salt = system_salt_;
}

void Mount::GetUserSalt(const Credentials& credentials, bool force,
                        SecureBlob* salt) const {
  FilePath path(GetUserSaltFile(credentials));
  crypto_->GetOrCreateSalt(path, CRYPTOHOME_DEFAULT_SALT_LENGTH, force, salt);
}

bool Mount::LoadFileBytes(const FilePath& path,
                          SecureBlob* blob) {
  int64 file_size;
  if (!file_util::PathExists(path)) {
    return false;
  }
  if (!file_util::GetFileSize(path, &file_size)) {
    LOG(ERROR) << "Could not get size of " << path.value();
    return false;
  }
  // Compare to the max of a 32-bit signed integer
  if (file_size > static_cast<int64>(INT_MAX)) {
    LOG(ERROR) << "File " << path.value() << " is too large: " << file_size;
    return false;
  }
  SecureBlob buf(file_size);
  int data_read = file_util::ReadFile(path, reinterpret_cast<char*>(&buf[0]),
                                      file_size);
  // Cast is okay because of comparison to INT_MAX above
  if (data_read != static_cast<int>(file_size)) {
    LOG(ERROR) << "Could not read entire file " << file_size;
    return false;
  }
  blob->swap(buf);
  return true;
}

bool Mount::LoadFileString(const FilePath& path,
                           std::string* content) {
  if (!file_util::PathExists(path)) {
    return false;
  }

  if (!file_util::ReadFileToString(path, content)) {
    LOG(INFO) << "Could not read file contents: " << path.value();
    return false;
  }

  return true;
}

bool Mount::SaveVaultKeysetOld(const Credentials& credentials,
                               const VaultKeyset& vault_keyset) const {
  // Get the vault keyset key
  SecureBlob user_salt;
  GetUserSalt(credentials, true, &user_salt);
  SecureBlob passkey;
  credentials.GetPasskey(&passkey);
  SecureBlob keyset_key;
  crypto_->PasskeyToKeysetKey(passkey, user_salt, 1, &keyset_key);

  // Encrypt the vault keyset
  SecureBlob salt(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  SecureBlob cipher_text;
  crypto_->GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                           salt.size());
  if (!crypto_->EncryptVaultKeysetOld(vault_keyset, keyset_key, salt,
                                      &cipher_text)) {
    LOG(ERROR) << "Encrypting vault keyset failed";
    return false;
  }

  // Save the master key
  unsigned int data_written = file_util::WriteFile(
      FilePath(GetUserKeyFile(credentials)),
      static_cast<const char*>(cipher_text.const_data()),
      cipher_text.size());

  if (data_written != cipher_text.size()) {
    LOG(ERROR) << "Write to master key failed";
    return false;
  }
  return true;
}

bool Mount::DecryptVaultKeysetOld(const Credentials& credentials,
                                  VaultKeyset* vault_keyset,
                                  MountError* error) const {
  // Generate the keyset key (key encryption key)
  SecureBlob user_salt;
  GetUserSalt(credentials, false, &user_salt);
  if (user_salt.size() == 0) {
    if (error) {
      *error = MOUNT_ERROR_FATAL;
    }
    return false;
  }
  SecureBlob passkey;
  credentials.GetPasskey(&passkey);
  SecureBlob keyset_key;
  crypto_->PasskeyToKeysetKey(passkey, user_salt, 1, &keyset_key);

  // Load the encrypted keyset
  FilePath user_key_file(GetUserKeyFile(credentials));
  if (!file_util::PathExists(user_key_file)) {
    if (error) {
      *error = MOUNT_ERROR_FATAL;
    }
    return false;
  }
  SecureBlob cipher_text;
  if (!LoadFileBytes(user_key_file, &cipher_text)) {
    if (error) {
      *error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // Attempt to unwrap the master key with the passkey key
  if (!crypto_->DecryptVaultKeysetOld(cipher_text, keyset_key, vault_keyset)) {
    if (error) {
      *error = Mount::MOUNT_ERROR_KEY_FAILURE;
    }
    return false;
  }

  return true;
}

bool Mount::EnsureDirHasOwner(const FilePath& fp, uid_t final_uid,
                              gid_t final_gid) const {
  std::vector<std::string> path_parts;
  fp.GetComponents(&path_parts);
  FilePath check_path("/");
  // Note that since check_path starts off at /, we don't want to re-append /,
  // which is path_parts[0]. Also, Append() DCHECKs if you pass it an absolute
  // path, which / is.
  for (size_t i = 1; i < path_parts.size(); i++) {
    check_path = check_path.Append(path_parts[i]);
    bool last = (i == (path_parts.size() - 1));
    uid_t uid = last ? final_uid : kMountOwnerUid;
    gid_t gid = last ? final_gid : kMountOwnerGid;
    struct stat st;
    if (!platform_->Stat(check_path.value(), &st)) {
      // Dirent not there, so create and set ownership.
      if (!platform_->CreateDirectory(check_path.value())) {
        PLOG(ERROR) << "Can't create: " << check_path.value();
        return false;
      }
      if (!platform_->SetOwnership(check_path.value(), uid, gid)) {
        PLOG(ERROR) << "Can't chown/chgrp: " << check_path.value()
                    << " uid " << uid << " gid " << gid;
        return false;
      }
    } else {
      // Dirent there; make sure it's acceptable.
      if (!S_ISDIR(st.st_mode)) {
        LOG(ERROR) << "Non-directory path: " << check_path.value();
        return false;
      }
      if (st.st_uid != uid) {
        LOG(ERROR) << "Owner mismatch: " << check_path.value()
                   << " " << st.st_uid << " != " << uid;
        return false;
      }
      if (st.st_gid != gid) {
        LOG(ERROR) << "Group mismatch: " << check_path.value()
                   << " " << st.st_gid << " != " << gid;
        return false;
      }
      if (st.st_mode & S_IWOTH) {
        LOG(ERROR) << "Permissions too lenient: " << check_path.value()
                   << " has " << std::oct << st.st_mode;
        return false;
      }
    }
  }
  return true;
}

bool Mount::EnsureUserMountPoints(const Credentials& credentials) const {
  const std::string username = credentials.GetFullUsernameString();
  FilePath root_path = chromeos::cryptohome::home::GetRootPath(username);
  FilePath user_path = chromeos::cryptohome::home::GetUserPath(username);
  if (!EnsureDirHasOwner(root_path, kMountOwnerUid, kMountOwnerGid)) {
    LOG(ERROR) << "Couldn't ensure root path: " << root_path.value();
    return false;
  }
  if (!EnsureDirHasOwner(user_path, default_user_, default_access_group_)) {
    LOG(ERROR) << "Couldn't ensure user path: " << user_path.value();
    return false;
  }
  return true;
}

} // namespace cryptohome
