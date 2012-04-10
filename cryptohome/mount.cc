// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

#include "mount.h"

#include <errno.h>
#include <sys/stat.h>

#include <base/bind.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <base/values.h>
#include <chromeos/cryptohome.h>
#include <chromeos/utility.h>
#include <set>

#include "crypto.h"
#include "cryptohome_common.h"
#include "homedirs.h"
#include "platform.h"
#include "username_passkey.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

using std::string;

namespace cryptohome {

const char kDefaultUserRoot[] = "/home/chronos";
const char kDefaultHomeDir[] = "/home/chronos/user";
const char kChapsTokenDir[] = "/home/chronos/user/.chaps";
const char kTokenSaltFile[] = "/home/chronos/user/.chaps/auth_data_salt";
const char kDefaultShadowRoot[] = "/home/.shadow";
const char kDefaultSharedUser[] = "chronos";
const char kChapsUserName[] = "chaps";
const char kDefaultSharedAccessGroup[] = "chronos-access";
const char kDefaultSkeletonSource[] = "/etc/skel";
const uid_t kMountOwnerUid = 0;
const gid_t kMountOwnerGid = 0;
// TODO(fes): Remove once UI for BWSI switches to MountGuest()
const char kIncognitoUser[] = "incognito";
// Encrypted files/directories in ecryptfs have file names that start with the
// following constant.  When clearing tracked subdirectories, we ignore these
// and only delete the pass-through directories.
const char kEncryptedFilePrefix[] = "ECRYPTFS_FNEK_ENCRYPTED.";
// Tracked directories - special sub-directories of the cryptohome
// vault, that are visible even if not mounted. Contents is still encrypted.
const char kVaultDir[] = "vault";
const char kCacheDir[] = "Cache";
const char kDownloadsDir[] = "Downloads";
const char kGCacheDir[] = "GCache";
const char kGCacheVersionDir[] = "v1";
const char kGCacheTmpDir[] = "tmp";
const char kUserHomeSuffix[] = "user";
const char kRootHomeSuffix[] = "root";
const char kMountDir[] = "mount";
const char kKeyFile[] = "master.0";
const char kEphemeralDir[] = "ephemeralfs";
const char kEphemeralMountType[] = "tmpfs";
const char kEphemeralMountPerms[] = "mode=0700";
const base::TimeDelta kOldUserLastActivityTime = base::TimeDelta::FromDays(92);

const char kDefaultEcryptfsCryptoAlg[] = "aes";
const int kDefaultEcryptfsKeySize = CRYPTOHOME_AES_KEY_BYTES;

// A helper class for scoping umask changes.
class ScopedUmask {
 public:
  ScopedUmask(Platform* platform, int mask)
      : platform_(platform),
        old_mask_(platform_->SetMask(mask)) {}
  ~ScopedUmask() {platform_->SetMask(old_mask_);}
 private:
  Platform* platform_;
  int old_mask_;
};

Mount::Mount()
    : default_user_(-1),
      chaps_user_(-1),
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
      use_tpm_(true),
      default_current_user_(new UserSession()),
      current_user_(default_current_user_.get()),
      default_user_timestamp_(new UserOldestActivityTimestampCache()),
      user_timestamp_(default_user_timestamp_.get()),
      enterprise_owned_(false),
      old_user_last_activity_time_(kOldUserLastActivityTime),
      pkcs11_state_(kUninitialized),
      is_pkcs11_passkey_migration_required_(false) {
}

Mount::~Mount() {
}

bool Mount::Init() {
  bool result = true;

  homedirs_.set_platform(platform_);
  homedirs_.set_shadow_root(shadow_root_);
  homedirs_.set_timestamp_cache(user_timestamp_);
  homedirs_.set_enterprise_owned(enterprise_owned_);
  homedirs_.set_old_user_last_activity_time(old_user_last_activity_time_);

  // Make sure both we and |homedirs_| have a proper device policy object.
  EnsureDevicePolicyLoaded(false);
  homedirs_.set_policy_provider(policy_provider_.get());
  homedirs_.set_crypto(crypto_);
  if (!homedirs_.Init())
    result = false;

  // Get the user id and group id of the default user
  if (!platform_->GetUserId(default_username_, &default_user_,
                           &default_group_)) {
    result = false;
  }

  // Get the user id of the chaps user.
  gid_t not_used;
  if (!platform_->GetUserId(kChapsUserName, &chaps_user_, &not_used)) {
    result = false;
  }

  // Get the group id of the default shared access group.
  if (!platform_->GetGroupId(kDefaultSharedAccessGroup,
                             &default_access_group_)) {
    result = false;
  }

  crypto_->set_use_tpm(use_tpm_);

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
  FilePath vault_path(GetUserVaultPath(
      credentials.GetObfuscatedUsername(system_salt_)));
  if (!file_util::DirectoryExists(vault_path)) {
    // If the vault directory doesn't exist, then create the cryptohome from
    // scratch
    bool result = CreateCryptohome(credentials);
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
  FilePath vault_path(GetUserVaultPath(
      credentials.GetObfuscatedUsername(system_salt_)));
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

  string username = credentials.GetFullUsernameString();
  if (username.compare(kIncognitoUser) == 0) {
    // TODO(fes): Have guest set error conditions?
    if (mount_error) {
      *mount_error = MOUNT_ERROR_NONE;
    }
    return MountGuestCryptohome();
  }

  ReloadDevicePolicy();
  bool ephemeral_users = AreEphemeralUsersEnabled();
  const string obfuscated_owner = GetObfuscatedOwner();
  if (ephemeral_users)
    RemoveNonOwnerCryptohomes();

  // Mount an ephemeral cryptohome if the ephemeral users policy is enabled and
  // the user is not the device owner.
  if (ephemeral_users &&
      (enterprise_owned_ || !obfuscated_owner.empty()) &&
      (credentials.GetObfuscatedUsername(system_salt_) != obfuscated_owner)) {
    if (!mount_args.create_if_missing) {
      LOG(ERROR) << "An ephemeral cryptohome can only be mounted when its "
                 << "creation on-the-fly is allowed.";
      *mount_error = MOUNT_ERROR_USER_DOES_NOT_EXIST;
      return false;
    }

    if (!MountEphemeralCryptohome(credentials)) {
      homedirs_.Remove(credentials.GetFullUsernameString());
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }

    current_user_->SetUser(credentials);
    *mount_error = MOUNT_ERROR_NONE;
    return true;
  }

  if (!mount_args.create_if_missing && !DoesCryptohomeExist(credentials)) {
    if (mount_error) {
      *mount_error = MOUNT_ERROR_USER_DOES_NOT_EXIST;
    }
    return false;
  }

  bool created = false;
  if (!EnsureCryptohome(credentials, &created)) {
    LOG(ERROR) << "Error creating cryptohome.";
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // Attempt to decrypt the vault keyset with the specified credentials
  VaultKeyset vault_keyset(platform_, crypto_);
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
      if (!homedirs_.Remove(credentials.GetFullUsernameString())) {
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

  string vault_path = GetUserVaultPath(
      credentials.GetObfuscatedUsername(system_salt_));
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

  if (!MountForUser(current_user_, vault_path, mount_point_, "ecryptfs",
                    ecryptfs_options)) {
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
  if (!BindForUser(current_user_, user_home, home_dir_)) {
    PLOG(ERROR) << "Bind mount failed: " << user_home << " -> " << home_dir_;
    UnmountAllForUser(current_user_);
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  string user_multi_home =
      chromeos::cryptohome::home::GetUserPath(username).value();
  if (!BindForUser(current_user_, user_home, user_multi_home)) {
    PLOG(ERROR) << "Bind mount failed: " << user_home << " -> "
                << user_multi_home;
    UnmountAllForUser(current_user_);
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  string root_home = GetMountedRootHomePath(credentials);
  string root_multi_home =
      chromeos::cryptohome::home::GetRootPath(username).value();
  if (!BindForUser(current_user_, root_home, root_multi_home)) {
    PLOG(ERROR) << "Bind mount failed: " << root_home << " -> "
                << root_multi_home;
    UnmountAllForUser(current_user_);
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // TODO(ellyjones): Expose the path to the root directory over dbus for use by
  // daemons. We may also want to bind-mount it somewhere stable.

  if (created) {
    CopySkeletonForUser(credentials);
  }

  if (!SetupGroupAccess()) {
    UnmountAllForUser(current_user_);
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  if (mount_error) {
    *mount_error = MOUNT_ERROR_NONE;
  }

  current_user_->SetUser(credentials);
  credentials.GetPasskey(&pkcs11_passkey_);
  return true;
}

bool Mount::MountEphemeralCryptohome(const Credentials& credentials) {
  const string username = credentials.GetFullUsernameString();
  const string path = GetUserEphemeralPath(
      credentials.GetObfuscatedUsername(system_salt_));
  const string user_multi_home =
      chromeos::cryptohome::home::GetUserPath(username).value();
  const string root_multi_home =
      chromeos::cryptohome::home::GetRootPath(username).value();
  if (!EnsureUserMountPoints(credentials))
    return false;
  if (!MountForUser(current_user_,
                    path,
                    user_multi_home,
                    kEphemeralMountType,
                    kEphemeralMountPerms)) {
    LOG(ERROR) << "Mount of ephemeral user home at " << user_multi_home
               << "failed: " << errno;
    return false;
  }
  if (!MountForUser(current_user_,
                    path,
                    root_multi_home,
                    kEphemeralMountType,
                    kEphemeralMountPerms)) {
    LOG(ERROR) << "Mount of ephemeral root home at " << root_multi_home
               << "failed: " << errno;
    UnmountAllForUser(current_user_);
    return false;
  }
  if (!BindForUser(current_user_, user_multi_home, home_dir_)) {
    LOG(ERROR) << "Bind mount of ephemeral user home from " << user_multi_home
               << " to " << home_dir_ << " failed: " << errno;
    UnmountAllForUser(current_user_);
    return false;
  }
  return SetUpEphemeralCryptohome(user_multi_home);
}

bool Mount::SetUpEphemeralCryptohome(const std::string& home_dir) {
  if (set_vault_ownership_) {
    if (!platform_->SetOwnership(home_dir, default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of path: " << home_dir;
      UnmountAllForUser(current_user_);
      return false;
    }
  }
  CopySkeleton();

  // Create the Downloads directory if it does not exist so that it can later be
  // made group accessible when SetupGroupAccess() is called.
  FilePath downloads_path = FilePath(home_dir).Append(kDownloadsDir);
  if (!file_util::DirectoryExists(downloads_path)) {
    if (!file_util::CreateDirectory(downloads_path) ||
        !platform_->SetOwnership(downloads_path.value(),
                                 default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't create user Downloads directory: "
                 << downloads_path.value();
      UnmountAllForUser(current_user_);
      return false;
    }
  }

  if (!SetupGroupAccess()) {
    UnmountAllForUser(current_user_);
    return false;
  }

  return true;
}

bool Mount::MountForUser(UserSession *user, const std::string& src,
                         const std::string& dest, const std::string& type,
                         const std::string& options) {
  if (platform_->Mount(src, dest, type, options)) {
    user->PushMount(dest);
    return true;
  }
  return false;
}

bool Mount::BindForUser(UserSession *user, const std::string& src,
                        const std::string& dest) {
  if (platform_->Bind(src, dest)) {
    user->PushMount(dest);
    return true;
  }
  return false;
}

bool Mount::UnmountForUser(UserSession *user) {
  std::string mount_point;
  if (!user->PopMount(&mount_point)) {
    return false;
  }
  ForceUnmount(mount_point);
  return true;
}

void Mount::UnmountAllForUser(UserSession *user) {
  while (UnmountForUser(user)) { }
}

void Mount::ForceUnmount(const std::string& mount_point) {
  // Try an immediate unmount
  bool was_busy;
  if (!platform_->Unmount(mount_point, false, &was_busy)) {
    LOG(ERROR) << "Couldn't unmount vault immediately, was_busy = " << was_busy;
    if (was_busy) {
      std::vector<ProcessInformation> processes;
      platform_->GetProcessesWithOpenFiles(mount_point, &processes);
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
    platform_->Unmount(mount_point, true, NULL);
    sync();
  }
}

void Mount::RemoveNonOwnerCryptohomesCallback(const FilePath& vault) {
  if (vault != FilePath(GetUserVaultPath(GetObfuscatedOwner())))
    file_util::Delete(vault.DirName(), true);
}

void Mount::RemoveNonOwnerDirectories(const FilePath& prefix) {
  file_util::FileEnumerator dir_enumerator(prefix, false,
      file_util::FileEnumerator::DIRECTORIES);
  for (FilePath next_path = dir_enumerator.Next(); !next_path.empty();
       next_path = dir_enumerator.Next()) {
    const std::string str_dir_name = next_path.BaseName().value();
    if (!base::strcasecmp(str_dir_name.c_str(), GetObfuscatedOwner().c_str()))
      continue;  // Skip the owner's directory.
    if (!chromeos::cryptohome::home::IsSanitizedUserName(str_dir_name))
      continue;  // Skip any directory whose name is not an obfuscated user
                 // name.
    if (platform_->IsDirectoryMounted(next_path.value()))
      continue;  // Skip any directory that is currently mounted.
    platform_->DeleteFile(next_path.value(), true);
  }
}

void Mount::RemoveNonOwnerCryptohomes() {
  if (!enterprise_owned_ && GetObfuscatedOwner().empty())
    return;

  DoForEveryUnmountedCryptohome(base::Bind(
      &Mount::RemoveNonOwnerCryptohomesCallback,
      base::Unretained(this)));
  RemoveNonOwnerDirectories(chromeos::cryptohome::home::GetUserPathPrefix());
  RemoveNonOwnerDirectories(chromeos::cryptohome::home::GetRootPathPrefix());
}

bool Mount::UnmountCryptohome() {
  RemovePkcs11Token();
  UnmountAllForUser(current_user_);
  ReloadDevicePolicy();
  if (AreEphemeralUsersEnabled())
    RemoveNonOwnerCryptohomes();
  else
    UpdateCurrentUserActivityTimestamp(0);
  current_user_->Reset();

  // Clear the user keyring if the unmount was successful
  crypto_->ClearKeyset();

  return true;
}

bool Mount::IsCryptohomeMounted() const {
  return platform_->IsDirectoryMounted(home_dir_);
}

bool Mount::IsCryptohomeMountedForUser(const Credentials& credentials) const {
  const std::string obfuscated_owner =
      credentials.GetObfuscatedUsername(system_salt_);
  return (platform_->IsDirectoryMountedWith(
              home_dir_,
              GetUserVaultPath(obfuscated_owner)) ||
          platform_->IsDirectoryMountedWith(
              home_dir_,
              GetUserEphemeralPath(obfuscated_owner)));
}

bool Mount::IsVaultMountedForUser(const Credentials& credentials) const {
  return platform_->IsDirectoryMountedWith(
      home_dir_,
      GetUserVaultPath(credentials.GetObfuscatedUsername(system_salt_)));
}

bool Mount::CreateCryptohome(const Credentials& credentials) const {
  int original_mask = platform_->SetMask(kDefaultUmask);

  // Create the user's entry in the shadow root
  FilePath user_dir(GetUserDirectory(credentials));
  file_util::CreateDirectory(user_dir);

  // Generate a new master key
  VaultKeyset vault_keyset(platform_, crypto_);
  vault_keyset.CreateRandom();
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
  std::string vault_path = GetUserVaultPath(
      credentials.GetObfuscatedUsername(system_salt_));
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
  const FilePath shadow_home(VaultPathToUserPath(GetUserVaultPath(
      credentials.GetObfuscatedUsername(system_salt_))));
  if (!file_util::DirectoryExists(shadow_home)) {
     LOG(ERROR) << "Can't create tracked subdirectories for a missing user.";
     platform_->SetMask(original_mask);
     return false;
  }

  const FilePath user_home(GetMountedUserHomePath(credentials));

  static const FilePath kTrackedDirs[] = {
    FilePath(kCacheDir),
    FilePath(kDownloadsDir),
    FilePath(kGCacheDir),
    FilePath(kGCacheDir).Append(kGCacheVersionDir),
    FilePath(kGCacheDir).Append(kGCacheVersionDir).Append(kGCacheTmpDir),
  };

  // The call is allowed to partially fail if directory creation fails, but we
  // want to have as many of the specified tracked directories created as
  // possible.
  bool result = true;
  for (size_t index = 0; index < arraysize(kTrackedDirs); ++index) {
    const FilePath shadowside_dir = shadow_home.Append(kTrackedDirs[index]);
    const FilePath userside_dir = user_home.Append(kTrackedDirs[index]);

    // If non-pass-through dir with the same name existed - delete it
    // to prevent duplication.
    if (!is_new && file_util::DirectoryExists(userside_dir) &&
        !file_util::DirectoryExists(shadowside_dir)) {
      file_util::Delete(userside_dir, true);
    }

    // Create pass-through directory.
    if (!file_util::DirectoryExists(shadowside_dir)) {
      LOG(INFO) << "Creating pass-through directories "
                << shadowside_dir.value();
      file_util::CreateDirectory(shadowside_dir);
      if (set_vault_ownership_) {
        if (!platform_->SetOwnership(shadowside_dir.value(),
                                     default_user_, default_group_)) {
          LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                     << default_group_ << ") of tracked directory path: "
                     << shadowside_dir.value();
          file_util::Delete(shadowside_dir, true);
          result = false;
          continue;
        }
      }
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

void Mount::EnsureDevicePolicyLoaded(bool force_reload) {
  if (!policy_provider_.get()) {
    policy_provider_.reset(new policy::PolicyProvider());
    homedirs_.set_policy_provider(policy_provider_.get());
  } else if (force_reload) {
    policy_provider_->Reload();
  }
}

void Mount::DoForEveryUnmountedCryptohome(
    const CryptohomeCallback& cryptohome_cb) {
  FilePath shadow_root(shadow_root_);
  file_util::FileEnumerator dir_enumerator(shadow_root, false,
      file_util::FileEnumerator::DIRECTORIES);
  for (FilePath next_path = dir_enumerator.Next(); !next_path.empty();
       next_path = dir_enumerator.Next()) {
    const std::string str_dir_name = next_path.BaseName().value();
    if (!chromeos::cryptohome::home::IsSanitizedUserName(str_dir_name))
      continue;
    const FilePath vault_path = next_path.Append(kVaultDir);
    if (!file_util::DirectoryExists(vault_path)) {
      continue;
    }
    if (platform_->IsDirectoryMountedWith(home_dir_, vault_path.value())) {
      continue;
    }
    cryptohome_cb.Run(vault_path);
  }
}

// Deletes specified directory contents, but leaves the directory itself.
static void DeleteDirectoryContents(const FilePath& dir) {
  file_util::FileEnumerator subdir_enumerator(
      dir,
      false,
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::SHOW_SYM_LINKS));
  for (FilePath subdir_path = subdir_enumerator.Next(); !subdir_path.empty();
       subdir_path = subdir_enumerator.Next()) {
    file_util::Delete(subdir_path, true);
  }
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
  return homedirs_.FreeDiskSpace();
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

bool Mount::AreSameUser(const Credentials& credentials) {
  return current_user_->CheckUser(credentials);
}

bool Mount::AreValid(const Credentials& credentials) {
  // If the current logged in user matches, use the UserSession to verify the
  // credentials.  This is less costly than a trip to the TPM, and only verifies
  // a user during their logged in session.
  if (current_user_->CheckUser(credentials)) {
    return current_user_->Verify(credentials);
  }
  return false;
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
          *error = MOUNT_ERROR_FATAL;
          break;
        case Crypto::CE_TPM_COMM_ERROR:
          *error = MOUNT_ERROR_TPM_COMM_ERROR;
          break;
        case Crypto::CE_TPM_DEFEND_LOCK:
          *error = MOUNT_ERROR_TPM_DEFEND_LOCK;
          break;
        default:
          *error = MOUNT_ERROR_KEY_FAILURE;
          break;
      }
    }
    return false;
  }

  if (!migrate_if_needed)
    return true;

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
  bool should_scrypt = true;
  do {
    // If the keyset was TPM-wrapped, but there was no public key hash,
    // always re-save.  Otherwise, check the table.
    if (crypto_error != Crypto::CE_NO_PUBLIC_KEY_HASH) {
      if (tpm_wrapped && should_tpm)
        break;  // 5, 8
      if (scrypt_wrapped && should_scrypt && !should_tpm)
        break;  // 12
      if (!tpm_wrapped && !scrypt_wrapped && !should_tpm && !should_scrypt)
        break;  // 1
    }
    // This is not considered a fatal error.  Re-saving with the desired
    // protection is ideal, but not required.
    SerializedVaultKeyset new_serialized;
    new_serialized.CopyFrom(*serialized);
    if (ReEncryptVaultKeyset(credentials, *vault_keyset, &new_serialized)) {
      serialized->CopyFrom(new_serialized);
    }
  } while (false);

  return true;
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
  if (!CacheOldFiles(files)) {
    LOG(ERROR) << "Couldn't cache old key material.";
    return false;
  }
  if (!AddVaultKeyset(credentials, vault_keyset, serialized)) {
    LOG(ERROR) << "Couldn't add keyset.";
    RevertCacheFiles(files);
    return false;
  }
  if (!StoreVaultKeyset(credentials, *serialized)) {
    LOG(ERROR) << "Write to master key failed";
    RevertCacheFiles(files);
    return false;
  }
  DeleteCacheFiles(files);
  return true;
}

bool Mount::MigratePasskey(const Credentials& credentials,
                           const char* old_key) {
  std::string username = credentials.GetFullUsernameString();
  UsernamePasskey old_credentials(username.c_str(),
                                  SecureBlob(old_key, strlen(old_key)));
  // Attempt to decrypt the vault keyset with the specified credentials
  MountError mount_error;
  VaultKeyset vault_keyset(platform_, crypto_);
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
    // Setup passkey migration for PKCS #11.
    credentials.GetPasskey(&pkcs11_passkey_);
    old_credentials.GetPasskey(&pkcs11_old_passkey_);
    is_pkcs11_passkey_migration_required_ = true;
    return true;
  }
  current_user_->Reset();
  return false;
}

bool Mount::MountGuestCryptohome() {
  current_user_->Reset();

  // Attempt to mount guestfs
  if (!MountForUser(current_user_, "guestfs", home_dir_, kEphemeralMountType,
                    kEphemeralMountPerms)) {
    LOG(ERROR) << "Cryptohome mount failed: " << errno << " for guestfs";
    return false;
  }
  return SetUpEphemeralCryptohome(home_dir_);
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
  return StringPrintf("%s/%s/%s",
                      shadow_root_.c_str(),
                      obfuscated_username.c_str(),
                      kKeyFile);
}

string Mount::GetUserEphemeralPath(const string& obfuscated_username) const {
  return StringPrintf("%s/%s", kEphemeralDir, obfuscated_username.c_str());
}

string Mount::GetUserVaultPath(const std::string& obfuscated_username) const {
  return StringPrintf("%s/%s/%s",
                      shadow_root_.c_str(),
                      obfuscated_username.c_str(),
                      kVaultDir);
}

string Mount::GetUserMountDirectory(const Credentials& credentials) const {
  return StringPrintf("%s/%s/%s",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str(),
                      kMountDir);
}

string Mount::VaultPathToUserPath(const std::string& vault) const {
  return StringPrintf("%s/%s", vault.c_str(), kUserHomeSuffix);
}

string Mount::VaultPathToRootPath(const std::string& vault) const {
  return StringPrintf("%s/%s", vault.c_str(), kRootHomeSuffix);
}

string Mount::GetMountedUserHomePath(const Credentials& credentials) const {
  return StringPrintf("%s/%s", GetUserMountDirectory(credentials).c_str(),
                      kUserHomeSuffix);
}

string Mount::GetMountedRootHomePath(const Credentials& credentials) const {
  return StringPrintf("%s/%s", GetUserMountDirectory(credentials).c_str(),
                      kRootHomeSuffix);
}

string Mount::GetObfuscatedOwner() {
  EnsureDevicePolicyLoaded(false);
  string owner;
  if (policy_provider_->device_policy_is_loaded())
    policy_provider_->GetDevicePolicy().GetOwner(&owner);

  if (!owner.empty()) {
    return UsernamePasskey(owner.c_str(), chromeos::Blob())
        .GetObfuscatedUsername(system_salt_);
  }
  return "";
}

bool Mount::AreEphemeralUsersEnabled() {
  EnsureDevicePolicyLoaded(false);
  // If the policy cannot be loaded, default to non-ephemeral users.
  bool ephemeral_users_enabled = false;
  if (policy_provider_->device_policy_is_loaded())
    policy_provider_->GetDevicePolicy().GetEphemeralUsersEnabled(
        &ephemeral_users_enabled);
  return ephemeral_users_enabled;
}

void Mount::ReloadDevicePolicy() {
  EnsureDevicePolicyLoaded(true);
}

void Mount::InsertPkcs11Token() {
  // If the Chaps database directory does not exist, create it.
  // TODO(dkrahn): If the directory exists we should check the ownership and
  // permissions and log a warning if they are not as expected. See
  // crosbug.com/28291.
  std::string chaps_dir = kChapsTokenDir;
  if (!platform_->DirectoryExists(kChapsTokenDir)) {
    if (!platform_->CreateDirectory(kChapsTokenDir)) {
      LOG(ERROR) << "Failed to create " << kChapsTokenDir;
      return;
    }
    if (!platform_->SetOwnership(kChapsTokenDir,
                                 chaps_user_,
                                 default_access_group_)) {
      LOG(ERROR) << "Couldn't set file ownership for " << kChapsTokenDir;
      return;
    }
    // 0750: u + rwx, g + rx
    mode_t chaps_perms = S_IRWXU | S_IRGRP | S_IXGRP;
    if (!platform_->SetPermissions(kChapsTokenDir, chaps_perms)) {
      LOG(ERROR) << "Couldn't set permissions for " << kChapsTokenDir;
      return;
    }
  }
  // We may create a salt file and, if so, we want to restrict access to it.
  ScopedUmask scoped_umask(platform_, kDefaultUmask);

  // Derive authorization data for the token from the passkey.
  FilePath salt_file(kTokenSaltFile);
  SecureBlob auth_data;
  if (!crypto_->PasskeyToTokenAuthData(pkcs11_passkey_, salt_file, &auth_data))
    return;
  // If migration is required, send it before the login event.
  if (is_pkcs11_passkey_migration_required_) {
    LOG(INFO) << "Migrating authorization data.";
    SecureBlob old_auth_data;
    if (!crypto_->PasskeyToTokenAuthData(pkcs11_old_passkey_,
                                         salt_file,
                                         &old_auth_data))
      return;
    chaps_event_client_.FireChangeAuthDataEvent(
        kChapsTokenDir,
        static_cast<const uint8_t*>(old_auth_data.const_data()),
        old_auth_data.size(),
        static_cast<const uint8_t*>(auth_data.const_data()),
        auth_data.size());
    is_pkcs11_passkey_migration_required_ = false;
    pkcs11_old_passkey_.clear_contents();
  }
  chaps_event_client_.FireLoginEvent(
      kChapsTokenDir,
      static_cast<const uint8_t*>(auth_data.const_data()),
      auth_data.size());
  pkcs11_passkey_.clear_contents();
}

void Mount::RemovePkcs11Token() {
  chaps_event_client_.FireLogoutEvent(kChapsTokenDir);
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

void Mount::MigrateToUserHome(const std::string& vault_path) const {
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

  if (!platform_->SetOwnership(user_path.value(), default_user_,
                               default_group_)) {
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

bool Mount::CacheOldFiles(const std::vector<std::string>& files) const {
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

bool Mount::RevertCacheFiles(const std::vector<std::string>& files) const {
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

bool Mount::DeleteCacheFiles(const std::vector<std::string>& files) const {
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

bool Mount::EnsureDirHasOwner(const FilePath& fp, uid_t final_uid,
                              gid_t final_gid) const {
  std::vector<std::string> path_parts;
  fp.GetComponents(&path_parts);
  // The path given should be absolute to that its first part is /. This is not
  // actually checked so that relative paths can be used during testing.
  FilePath check_path(path_parts[0]);
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

Value* Mount::GetStatus() {
  std::string user;
  SerializedVaultKeyset keyset;
  DictionaryValue* dv = new DictionaryValue();
  current_user_->GetObfuscatedUsername(&user);
  ListValue* keysets = new ListValue();
  if (user.length()) {
    DictionaryValue* keyset0 = new DictionaryValue();
    if (LoadVaultKeysetForUser(user, &keyset)) {
      bool tpm = keyset.flags() & SerializedVaultKeyset::TPM_WRAPPED;
      bool scrypt = keyset.flags() & SerializedVaultKeyset::SCRYPT_WRAPPED;
      keyset0->SetBoolean("tpm", tpm);
      keyset0->SetBoolean("scrypt", scrypt);
      keyset0->SetBoolean("ok", true);
    } else {
      keyset0->SetBoolean("ok", false);
    }
    keysets->Append(keyset0);
  }
  dv->Set("keysets", keysets);
  dv->SetBoolean("mounted", IsCryptohomeMounted());
  dv->SetString("owner", GetObfuscatedOwner());
  dv->SetBoolean("enterprise", enterprise_owned());
  return dv;
}

}  // namespace cryptohome
