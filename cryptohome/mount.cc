// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

#include "mount.h"

#include <errno.h>
#include <sys/stat.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <base/values.h>
#include <chaps/isolate.h>
#include <chaps/token_manager_client.h>
#include <chromeos/cryptohome.h>
#include <chromeos/utility.h>
#include <set>

#include "crypto.h"
#include "cryptohome_common.h"
#include "cryptolib.h"
#include "homedirs.h"
#include "pkcs11_init.h"
#include "platform.h"
#include "username_passkey.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

using chaps::IsolateCredentialManager;
using chromeos::SecureBlob;
using std::string;

namespace cryptohome {

const char kDefaultHomeDir[] = "/home/chronos/user";
const char kDefaultShadowRoot[] = "/home/.shadow";
const char kDefaultSharedUser[] = "chronos";
const char kChapsUserName[] = "chaps";
const char kDefaultSharedAccessGroup[] = "chronos-access";
const char kDefaultSkeletonSource[] = "/etc/skel";
const uid_t kMountOwnerUid = 0;
const gid_t kMountOwnerGid = 0;
// TODO(fes): Remove once UI for BWSI switches to MountGuest()
const char kIncognitoUser[] = "incognito";
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
const char kSkeletonDir[] = "skeleton";
const char kKeyFile[] = "master.";
const int kKeyFileMax = 100;  // master.0 ... master.99
const char kEphemeralDir[] = "ephemeralfs";
const char kEphemeralMountType[] = "tmpfs";
const char kGuestMountPath[] = "guestfs";
const char kEphemeralMountPerms[] = "mode=0700";
const base::TimeDelta kOldUserLastActivityTime = base::TimeDelta::FromDays(92);

const int kDefaultEcryptfsKeySize = CRYPTOHOME_AES_KEY_BYTES;
const gid_t kDaemonStoreGid = 400;

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

Mount::ScopedMountPoint::ScopedMountPoint(Mount* mount,
                                          const string& path)
  : mount_(mount), path_(path) {
}

Mount::ScopedMountPoint::~ScopedMountPoint() {
  if (mount_->platform_->IsDirectoryMounted(path_)) {
    mount_->ForceUnmount(path_);
  }
}

Mount::Mount()
    : default_user_(-1),
      chaps_user_(-1),
      default_group_(-1),
      default_access_group_(-1),
      shadow_root_(kDefaultShadowRoot),
      skel_source_(kDefaultSkeletonSource),
      system_salt_(),
      default_platform_(new Platform()),
      platform_(default_platform_.get()),
      default_crypto_(new Crypto(platform_)),
      crypto_(default_crypto_.get()),
      default_homedirs_(new HomeDirs()),
      homedirs_(default_homedirs_.get()),
      use_tpm_(true),
      default_current_user_(new UserSession()),
      current_user_(default_current_user_.get()),
      default_user_timestamp_(new UserOldestActivityTimestampCache()),
      user_timestamp_(default_user_timestamp_.get()),
      enterprise_owned_(false),
      old_user_last_activity_time_(kOldUserLastActivityTime),
      pkcs11_state_(kUninitialized),
      is_pkcs11_passkey_migration_required_(false),
      legacy_mount_(true),
      ephemeral_mount_(false) {
}

Mount::~Mount() {
  if (IsMounted())
    UnmountCryptohome();
}

bool Mount::Init() {
  bool result = true;

  homedirs_->set_platform(platform_);
  homedirs_->set_shadow_root(shadow_root_);
  homedirs_->set_timestamp_cache(user_timestamp_);
  homedirs_->set_enterprise_owned(enterprise_owned_);
  homedirs_->set_old_user_last_activity_time(old_user_last_activity_time_);

  // Make sure both we and |homedirs_| have a proper device policy object.
  EnsureDevicePolicyLoaded(false);
  homedirs_->set_policy_provider(policy_provider_.get());
  homedirs_->set_crypto(crypto_);
  if (!homedirs_->Init())
    result = false;

  // Get the user id and group id of the default user
  if (!platform_->GetUserId(kDefaultSharedUser, &default_user_,
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
  if (!platform_->DirectoryExists(shadow_root_)) {
    platform_->CreateDirectory(shadow_root_);
  }

  if (!crypto_->Init()) {
    LOG(ERROR) << "Failed to initialize a Crypto instance";
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

  current_user_->Init(system_salt_);

  return result;
}

bool Mount::EnsureCryptohome(const Credentials& credentials,
                             bool* created) const {
  // If the user has an old-style cryptohome, delete it
  const std::string old_image_path(StringPrintf("%s/image",
    GetUserDirectory(credentials).c_str()));
  if (platform_->FileExists(old_image_path)) {
    platform_->DeleteFile(GetUserDirectory(credentials), true);
  }
  if (!EnsureUserMountPoints(credentials)) {
    return false;
  }
  // Now check for the presence of a vault directory
  const std::string vault_path(GetUserVaultPath(
      credentials.GetObfuscatedUsername(system_salt_)));
  if (!platform_->DirectoryExists(vault_path)) {
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
  const std::string vault_path(GetUserVaultPath(
      credentials.GetObfuscatedUsername(system_salt_)));
  return platform_->DirectoryExists(vault_path);
}

bool Mount::MountCryptohome(const Credentials& credentials,
                            const Mount::MountArgs& mount_args,
                            MountError* mount_error) {
  if (IsMounted()) {
    *mount_error = MOUNT_ERROR_MOUNT_POINT_BUSY;
    return false;
  }

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

  string username = credentials.username();
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
    homedirs_->RemoveNonOwnerCryptohomes();

  bool non_owner = enterprise_owned_ || (!obfuscated_owner.empty() &&
      credentials.GetObfuscatedUsername(system_salt_) != obfuscated_owner);

  // If the user is not the owner and either the ephemeral users policy is
  // enabled or the |ensure_ephemeral| flag is set in the |mount_args|, mount an
  // ephemeral cryptohome.
  if (non_owner && (ephemeral_users || mount_args.ensure_ephemeral)) {
    if (!mount_args.create_if_missing) {
      LOG(ERROR) << "An ephemeral cryptohome can only be mounted when its "
                 << "creation on-the-fly is allowed.";
      *mount_error = MOUNT_ERROR_USER_DOES_NOT_EXIST;
      return false;
    }

    if (!MountEphemeralCryptohome(credentials)) {
      homedirs_->Remove(credentials.username());
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }

    // Ephemeral and guest users will not have a key index.
    current_user_->SetUser(credentials);
    *mount_error = MOUNT_ERROR_NONE;
    return true;
  }

  // If the |ensure_ephemeral| flag is set in the |mount_args|, never mount a
  // non-ephemeral cryptohome. Fail with an error instead.
  if (mount_args.ensure_ephemeral) {
    LOG(ERROR) << "An ephemeral cryptohome can only be mounted when the user "
               << "is not the owner.";
    *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }

  if (!mount_args.create_if_missing && !DoesCryptohomeExist(credentials)) {
    if (mount_error) {
      LOG(ERROR) << "Asked to mount nonexistent user";
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
  VaultKeyset vault_keyset;
  vault_keyset.Initialize(platform_, crypto_);
  SerializedVaultKeyset serialized;
  MountError local_mount_error = MOUNT_ERROR_NONE;
  int index = -1;
  if (!DecryptVaultKeyset(credentials, true, &vault_keyset, &serialized,
                          &index, &local_mount_error)) {
    if (mount_error) {
      *mount_error = local_mount_error;
    }
    if (recreate_decrypt_fatal & (local_mount_error & MOUNT_ERROR_FATAL)) {
      LOG(ERROR) << "Error, cryptohome must be re-created because of fatal "
                 << "error.";
      if (!homedirs_->Remove(credentials.username())) {
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

  string obfuscated_username = credentials.GetObfuscatedUsername(system_salt_);
  string vault_path = GetUserVaultPath(obfuscated_username);
  // Create vault_path/user as a passthrough directory, move all the (encrypted)
  // contents of vault_path into vault_path/user, create vault_path/root.
  MigrateToUserHome(vault_path);

  // TODO(wad) Make mount_point_ not instance-wide or do it at Init time.
  mount_point_ = GetUserMountDirectory(obfuscated_username);
  if (!platform_->CreateDirectory(mount_point_)) {
    PLOG(ERROR) << "Directory creation failed for " << mount_point_;
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // Since Service::Mount cleans up stale mounts, we should only reach
  // this point if someone attempts to re-mount an in-use mount point.
  if (platform_->IsDirectoryMounted(mount_point_)) {
    LOG(ERROR) << "Mount point is busy: " << mount_point_
               << " for " << vault_path;
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }
  // TODO(wad,ellyjones) Why does Mount take current_user_?
  if (!MountForUser(current_user_, vault_path, mount_point_, "ecryptfs",
                    ecryptfs_options)) {
    PLOG(ERROR) << "Cryptohome mount failed for vault " << vault_path;
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // Set the current user here so we can rely on it in the helpers..
  // On failure, they will linger, but should be reset on a new MountCryptohome
  // request.
  current_user_->SetUser(credentials);
  current_user_->set_key_index(index);

  // Move the tracked subdirectories from mount_point_/user to vault_path
  // as passthrough directories.
  CreateTrackedSubdirectories(credentials, created);

  if (created)
    CopySkeleton();

  string user_home = GetMountedUserHomePath(obfuscated_username);
  if (!SetupGroupAccess(FilePath(user_home))) {
    UnmountAllForUser(current_user_);
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  if (legacy_mount_)
    MountLegacyHome(user_home, mount_error);

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

  // Temporary while we do the migration involved in http://crbug.com/224291
  // TODO(ellyjones): remove this to fix http://crbug.com/229411
  string temp_multi_home = GetNewUserPath(username);
  if (!BindForUser(current_user_, user_home, temp_multi_home)) {
    PLOG(ERROR) << "Bind mount failed: " << user_home << " -> "
                << temp_multi_home;
    UnmountAllForUser(current_user_);
    if (mount_error)
      *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }

  string root_home = GetMountedRootHomePath(obfuscated_username);
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

  if (mount_error) {
    *mount_error = MOUNT_ERROR_NONE;
  }

  credentials.GetPasskey(&pkcs11_passkey_);
  return true;
}

bool Mount::MountEphemeralCryptohome(const Credentials& credentials) {
  const string username = credentials.username();
  string path = GetUserEphemeralPath(credentials.GetObfuscatedUsername(
      system_salt_));
  const string user_multi_home =
      chromeos::cryptohome::home::GetUserPath(username).value();
  const string root_multi_home =
      chromeos::cryptohome::home::GetRootPath(username).value();

  // If we're mounting as a guest, as source use just "guestfs" instead of an
  // actual path. We don't want the guest cryptohome to persist even between
  // logins during the same boot.
  if (credentials.username() == chromeos::cryptohome::home::kGuestUserName)
    path = kGuestMountPath;

  if (!EnsureUserMountPoints(credentials))
    return false;
  if (!SetUpEphemeralCryptohome(path, user_multi_home))
    return false;
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

  if (legacy_mount_)
    MountLegacyHome(user_multi_home, NULL);

  string temp_multi_home = GetNewUserPath(username);
  if (!BindForUser(current_user_, user_multi_home, temp_multi_home)) {
    PLOG(ERROR) << "Bind mount failed: " << user_multi_home << " -> "
                << temp_multi_home;
    UnmountAllForUser(current_user_);
    return false;
  }
  ephemeral_mount_ = true;
  return true;
}

bool Mount::SetUpEphemeralCryptohome(const string& source_path,
                                     const string& home_dir) {
  // First, build up the home dir at a mount point not accessible to chronos.
  // This helps to avoid chown race conditions.
  const string ephemeral_skeleton_path = GetEphemeralSkeletonPath();
  if (!platform_->CreateDirectory(ephemeral_skeleton_path)) {
    LOG(ERROR) << "Failed to create " << ephemeral_skeleton_path << ": "
               << errno;
    return false;
  }
  // Note! This mount point does not show up in the MountStack.
  // TODO(wad) check for existing mount first.
  if (!platform_->Mount(source_path,
                        ephemeral_skeleton_path,
                        kEphemeralMountType,
                        kEphemeralMountPerms)) {
    LOG(ERROR) << "Mount of ephemeral skeleton at " << ephemeral_skeleton_path
               << "failed: " << errno;
    return false;
  }
  // Whatever happens, we want to unmount the tmpfs used to build the skeleton
  // home directory.
  ScopedMountPoint scoped_skeleton_mount(this, ephemeral_skeleton_path);
  CopySkeleton();

  // Create the Downloads directory if it does not exist so that it can later be
  // made group accessible when SetupGroupAccess() is called.
  FilePath downloads_path =
      FilePath(ephemeral_skeleton_path).Append(kDownloadsDir);
  if (!platform_->DirectoryExists(downloads_path.value())) {
    if (!platform_->CreateDirectory(downloads_path.value()) ||
        !platform_->SetOwnership(downloads_path.value(),
                                 default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't create user Downloads directory: "
                 << downloads_path.value();
      return false;
    }
  }

  if (!platform_->SetOwnership(ephemeral_skeleton_path,
                               default_user_,
                               default_access_group_)) {
    LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
               << default_access_group_ << ") of path: "
               << ephemeral_skeleton_path;
    return false;
  }

  if (!SetupGroupAccess(FilePath(ephemeral_skeleton_path)))
    return false;

  if (!BindForUser(current_user_, ephemeral_skeleton_path, home_dir)) {
    LOG(ERROR) << "Bind mount of ephemeral user home from "
               << ephemeral_skeleton_path << " to " << home_dir << " failed: "
               << errno;
    return false;
  }
  return true;
}

bool Mount::MountForUser(UserSession *user, const std::string& src,
                         const std::string& dest, const std::string& type,
                         const std::string& options) {
  if (platform_->Mount(src, dest, type, options)) {
    mounts_.Push(dest);
    return true;
  }
  return false;
}

bool Mount::BindForUser(UserSession *user, const std::string& src,
                        const std::string& dest) {
  if (platform_->Bind(src, dest)) {
    mounts_.Push(dest);
    return true;
  }
  return false;
}

bool Mount::UnmountForUser(UserSession *user) {
  std::string mount_point;
  if (!mounts_.Pop(&mount_point)) {
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

bool Mount::UnmountCryptohome() {
  UnmountAllForUser(current_user_);
  ReloadDevicePolicy();
  if (AreEphemeralUsersEnabled())
    homedirs_->RemoveNonOwnerCryptohomes();
  else
    UpdateCurrentUserActivityTimestamp(0);

  RemovePkcs11Token();
  current_user_->Reset();
  ephemeral_mount_ = false;
  crypto_->ClearKeyset();

  return true;
}

bool Mount::IsMounted() const {
  return mounts_.size() != 0;
}

bool Mount::IsVaultMounted() const {
  string obfuscated_username;
  current_user_->GetObfuscatedUsername(&obfuscated_username);
  const std::string vault_path = GetUserMountDirectory(obfuscated_username);
  return mounts_.Contains(vault_path);
}

bool Mount::OwnsMountPoint(const std::string& path) const {
  return mounts_.Contains(path);
}

bool Mount::CreateCryptohome(const Credentials& credentials) const {
  int original_mask = platform_->SetMask(kDefaultUmask);

  // Create the user's entry in the shadow root
  FilePath user_dir(GetUserDirectory(credentials));
  platform_->CreateDirectory(user_dir.value());

  // Generate a new master key
  VaultKeyset vault_keyset;
  vault_keyset.Initialize(platform_, crypto_);
  vault_keyset.CreateRandom();
  SerializedVaultKeyset serialized;
  if (!AddVaultKeyset(credentials, vault_keyset, &serialized)) {
    platform_->SetMask(original_mask);
    LOG(ERROR) << "Failed to add vault keyset to new user";
    return false;
  }
  if (!StoreVaultKeysetForUser(
       credentials.GetObfuscatedUsername(system_salt_),
       0, // first key
       serialized)) {
    platform_->SetMask(original_mask);
    LOG(ERROR) << "Failed to store vault keyset for new user";
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
  const std::string obfuscated_username =
      credentials.GetObfuscatedUsername(system_salt_);
  const FilePath shadow_home(VaultPathToUserPath(GetUserVaultPath(
      obfuscated_username)));
  if (!platform_->DirectoryExists(shadow_home.value())) {
     LOG(ERROR) << "Can't create tracked subdirectories for a missing user.";
     platform_->SetMask(original_mask);
     return false;
  }

  const FilePath user_home(GetMountedUserHomePath(obfuscated_username));

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
    if (!is_new && platform_->DirectoryExists(userside_dir.value()) &&
        !platform_->DirectoryExists(shadowside_dir.value())) {
      platform_->DeleteFile(userside_dir.value(), true);
    }

    // Create pass-through directory.
    if (!platform_->DirectoryExists(shadowside_dir.value())) {
      LOG(INFO) << "Creating pass-through directories "
                << shadowside_dir.value();
      platform_->CreateDirectory(shadowside_dir.value());
      if (!platform_->SetOwnership(shadowside_dir.value(),
                                   default_user_, default_group_)) {
        LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                   << default_group_ << ") of tracked directory path: "
                   << shadowside_dir.value();
        platform_->DeleteFile(shadowside_dir.value(), true);
        result = false;
        continue;
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
  if (!obfuscated_username.empty() && !ephemeral_mount_) {
    SerializedVaultKeyset serialized;
    LoadVaultKeysetForUser(obfuscated_username, current_user_->key_index(),
                           &serialized);
    base::Time timestamp = platform_->GetCurrentTime();
    if (time_shift_sec > 0)
      timestamp -= base::TimeDelta::FromSeconds(time_shift_sec);
    serialized.set_last_activity_timestamp(timestamp.ToInternalValue());
    // Only update the key in use.
    StoreVaultKeysetForUser(obfuscated_username, current_user_->key_index(),
                            serialized);
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
    homedirs_->set_policy_provider(policy_provider_.get());
  } else if (force_reload) {
    policy_provider_->Reload();
  }
}

void Mount::DoForEveryUnmountedCryptohome(
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

bool Mount::SetupGroupAccess(const FilePath& home_dir) const {
  // Make the following directories group accessible by other system daemons:
  //   {home_dir}
  //   {home_dir}/Downloads
  //   {home_dir}/GCache (only if it exists)
  //   {home_dir}/GCache/v1 (only if it exists)
  const struct {
    FilePath path;
    bool optional;
  } kGroupAccessiblePaths[] = {
    { home_dir },
    { home_dir.Append(kDownloadsDir), false },
    { home_dir.Append(kGCacheDir), true },
    { home_dir.Append(kGCacheDir).Append(kGCacheVersionDir), true },
  };

  mode_t mode = S_IXGRP;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kGroupAccessiblePaths); ++i) {
    if (!platform_->FileExists(kGroupAccessiblePaths[i].path.value()) &&
        kGroupAccessiblePaths[i].optional)
      continue;

    if (!platform_->SetGroupAccessible(kGroupAccessiblePaths[i].path.value(),
                                       default_access_group_, mode))
      return false;
  }
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
                            int index,
                            SerializedVaultKeyset* serialized) const {
  return LoadVaultKeysetForUser(credentials.GetObfuscatedUsername(system_salt_),
                                index,
                                serialized);
}

bool Mount::LoadVaultKeysetForUser(const string& obfuscated_username,
                                   int index,
                                   SerializedVaultKeyset* serialized) const {
  if (index < 0 || index > kKeyFileMax) {
    LOG(ERROR) << "Attempted to load an invalid key index: " << index;
    return false;
  }
  // Load the encrypted keyset
  std::string user_key_file = GetUserKeyFileForUser(obfuscated_username, index);
  if (!platform_->FileExists(user_key_file)) {
    return false;
  }
  SecureBlob cipher_text;
  if (!platform_->ReadFile(user_key_file, &cipher_text)) {
    LOG(ERROR) << "Failed to read keyset file for user " << obfuscated_username;
    return false;
  }
  if (!serialized->ParseFromArray(
           static_cast<const unsigned char*>(cipher_text.data()),
           cipher_text.size())) {
    LOG(ERROR) << "Failed to parse keyset for user " << obfuscated_username;
    return false;
  }
  return true;
}

bool Mount::StoreVaultKeysetForUser(
    const string& obfuscated_username,
    int index,
    const SerializedVaultKeyset& serialized) const {
  if (index < 0 || index > kKeyFileMax) {
    LOG(ERROR) << "Attempted to store an invalid key index: " << index;
    return false;
  }
  SecureBlob final_blob(serialized.ByteSize());
  serialized.SerializeWithCachedSizesToArray(
      static_cast<google::protobuf::uint8*>(final_blob.data()));
  return platform_->WriteFile(GetUserKeyFileForUser(obfuscated_username, index),
                              final_blob);
}

bool Mount::DecryptVaultKeyset(const Credentials& credentials,
                               bool migrate_if_needed,
                               VaultKeyset* vault_keyset,
                               SerializedVaultKeyset* serialized,
                               int* index,
                               MountError* error) const {
  SecureBlob passkey;
  credentials.GetPasskey(&passkey);
  *error = MOUNT_ERROR_FATAL;

  std::string obfuscated_username =
    credentials.GetObfuscatedUsername(system_salt_);

  // Most straightforward approach, try every key. We can optimize
  // by walking the directory, but 100 failed open() calls isn't that
  // many on a sign-in failure.
  unsigned int crypt_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  *index = -1;
  std::vector<int> key_indices;
  if (!homedirs_->GetVaultKeysets(obfuscated_username, &key_indices)) {
    LOG(WARNING) << "No valid keysets on disk for " << obfuscated_username;
  }
  std::vector<int>::const_iterator iter = key_indices.begin();
  for ( ;iter != key_indices.end(); ++iter) {
    // Load the encrypted keyset
    if (!LoadVaultKeysetForUser(obfuscated_username, *iter, serialized)) {
      LOG(ERROR) << "Could not parse keyset " << *iter
                 << " for " << obfuscated_username;
      continue;
    }

    // Attempt decrypt the master key with the passkey
    crypt_flags = 0;
    crypto_error = Crypto::CE_NONE;
    if (crypto_->DecryptVaultKeyset(*serialized, passkey, &crypt_flags,
                                    &crypto_error, vault_keyset)) {
      // Success!
      *error = MOUNT_ERROR_NONE;
      *index = *iter;
      break;
    }

    if (error) {
      switch (crypto_error) {
        case Crypto::CE_TPM_FATAL:
        case Crypto::CE_OTHER_FATAL:
          *error = MOUNT_ERROR_FATAL;
          break;
        // Don't keep trying on TPM errors.
        case Crypto::CE_TPM_COMM_ERROR:
          *error = MOUNT_ERROR_TPM_COMM_ERROR;
          return false;
        case Crypto::CE_TPM_DEFEND_LOCK:
          *error = MOUNT_ERROR_TPM_DEFEND_LOCK;
          return false;
        case Crypto::CE_TPM_REBOOT:
          *error = MOUNT_ERROR_TPM_NEEDS_REBOOT;
          return false;
        default:
          *error = MOUNT_ERROR_KEY_FAILURE;
          break;
      }
    }
  }
  // Failed to decrypt any keyset.
  if (*error != MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Failed to decrypt any keysets for " << obfuscated_username;
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
    if (ReEncryptVaultKeyset(credentials, *vault_keyset, *index,
                             &new_serialized)) {
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
  CryptoLib::GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                             salt.size());

  if (!crypto_->EncryptVaultKeyset(vault_keyset, passkey, salt, serialized)) {
    LOG(ERROR) << "Encrypting vault keyset failed";
    return false;
  }

  return true;
}

bool Mount::ReEncryptVaultKeyset(const Credentials& credentials,
                                 const VaultKeyset& vault_keyset,
                                 int key_index,
                              SerializedVaultKeyset* serialized) const {
  std::string obfuscated_username =
    credentials.GetObfuscatedUsername(system_salt_);
  std::vector<std::string> files(2);
  files[0] = GetUserSaltFileForUser(obfuscated_username, key_index);
  files[1] = GetUserKeyFileForUser(obfuscated_username, key_index);
  if (!CacheOldFiles(files)) {
    LOG(ERROR) << "Couldn't cache old key material.";
    return false;
  }
  if (!AddVaultKeyset(credentials, vault_keyset, serialized)) {
    LOG(ERROR) << "Couldn't add keyset.";
    RevertCacheFiles(files);
    return false;
  }
  if (!StoreVaultKeysetForUser(credentials.GetObfuscatedUsername(system_salt_),
                               key_index, *serialized)) {
    LOG(ERROR) << "Write to master key failed";
    RevertCacheFiles(files);
    return false;
  }
  DeleteCacheFiles(files);
  return true;
}

bool Mount::MountGuestCryptohome() {
  std::string guest = chromeos::cryptohome::home::kGuestUserName;
  UsernamePasskey guest_creds(guest.c_str(), chromeos::Blob(0));
  current_user_->Reset();
  return MountEphemeralCryptohome(guest_creds);
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

string Mount::GetUserSaltFileForUser(const string& obfuscated_username,
                                    int index) const {
  DCHECK(index < kKeyFileMax && index >= 0);
  return StringPrintf("%s/%s/master.%d.salt",
                      shadow_root_.c_str(),
                      obfuscated_username.c_str(),
                      index);
}

string Mount::GetUserKeyFileForUser(const string& obfuscated_username,
                                    int index) const {
  DCHECK(index < kKeyFileMax && index >= 0);
  return StringPrintf("%s/%s/%s%d",
                      shadow_root_.c_str(),
                      obfuscated_username.c_str(),
                      kKeyFile,
                      index);
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

string Mount::GetUserMountDirectory(
    const std::string& obfuscated_username) const {
  return StringPrintf("%s/%s/%s",
                      shadow_root_.c_str(),
                      obfuscated_username.c_str(),
                      kMountDir);
}

string Mount::VaultPathToUserPath(const std::string& vault) const {
  return StringPrintf("%s/%s", vault.c_str(), kUserHomeSuffix);
}

string Mount::VaultPathToRootPath(const std::string& vault) const {
  return StringPrintf("%s/%s", vault.c_str(), kRootHomeSuffix);
}

string Mount::GetMountedUserHomePath(
    const std::string& obfuscated_username) const {
  return StringPrintf("%s/%s",
                      GetUserMountDirectory(obfuscated_username).c_str(),
                      kUserHomeSuffix);
}

string Mount::GetMountedRootHomePath(
    const std::string& obfuscated_username) const {
  return StringPrintf("%s/%s",
                      GetUserMountDirectory(obfuscated_username).c_str(),
                      kRootHomeSuffix);
}

// TODO(dkrahn,wad) Makes this unique so we don't have to worry about
//                  parallelism.
string Mount::GetEphemeralSkeletonPath() const {
  return StringPrintf("%s/%s", shadow_root_.c_str(), kSkeletonDir);
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

bool Mount::CheckChapsDirectory(const std::string& dir,
                                const std::string& legacy_dir) {
  const Platform::Permissions kChapsDirPermissions = {
    chaps_user_,                 // chaps
    default_access_group_,       // chronos-access
    S_IRWXU | S_IRGRP | S_IXGRP  // 0750
  };
  const Platform::Permissions kChapsFilePermissions = {
    chaps_user_,                 // chaps
    default_access_group_,       // chronos-access
    S_IRUSR | S_IWUSR | S_IRGRP  // 0640
  };
  const Platform::Permissions kChapsSaltPermissions = {
    0,                 // root
    0,                 // root
    S_IRUSR | S_IWUSR  // 0600
  };

  // If the Chaps database directory does not exist, create it.
  if (!platform_->DirectoryExists(dir)) {
    if (platform_->DirectoryExists(legacy_dir)) {
      LOG(INFO) << "Moving chaps directory from " << legacy_dir << " to "
                << dir;
      if (!platform_->CopyWithPermissions(legacy_dir, dir)) {
        return false;
      }
      if (!platform_->DeleteFile(legacy_dir, true)) {
        PLOG(WARNING) << "Failed to clean up " << legacy_dir;
        return false;
      }
    } else {
      if (!platform_->CreateDirectory(dir)) {
        LOG(ERROR) << "Failed to create " << dir;
        return false;
      }
      if (!platform_->SetOwnership(dir,
                                   kChapsDirPermissions.user,
                                   kChapsDirPermissions.group)) {
        LOG(ERROR) << "Couldn't set file ownership for " << dir;
        return false;
      }
      if (!platform_->SetPermissions(dir, kChapsDirPermissions.mode)) {
        LOG(ERROR) << "Couldn't set permissions for " << dir;
        return false;
      }
    }
    return true;
  }
  // Directory already exists so check permissions and log a warning
  // if not as expected then attempt to apply correct permissions.
  std::map<std::string, Platform::Permissions> special_cases;
  special_cases[dir + "/auth_data_salt"] = kChapsSaltPermissions;
  if (!platform_->ApplyPermissionsRecursive(dir,
                                            kChapsFilePermissions,
                                            kChapsDirPermissions,
                                            special_cases)) {
    LOG(ERROR) << "Chaps permissions failure.";
    return false;
  }
  return true;
}

bool Mount::InsertPkcs11Token() {
  std::string username = current_user_->username();
  FilePath token_dir = homedirs_->GetChapsTokenDir(username);
  FilePath legacy_token_dir = homedirs_->GetLegacyChapsTokenDir(username);

  if (!CheckChapsDirectory(token_dir.value(),
                           legacy_token_dir.value()))
    return false;
  // We may create a salt file and, if so, we want to restrict access to it.
  ScopedUmask scoped_umask(platform_, kDefaultUmask);

  // Derive authorization data for the token from the passkey.
  FilePath salt_file = homedirs_->GetChapsTokenSaltPath(username);
  SecureBlob auth_data;
  if (!crypto_->PasskeyToTokenAuthData(pkcs11_passkey_, salt_file, &auth_data))
    return false;
  chaps::TokenManagerClient chaps_client;
  // If migration is required, send it before the login event.
  if (is_pkcs11_passkey_migration_required_) {
    LOG(INFO) << "Migrating authorization data.";
    SecureBlob old_auth_data;
    if (!crypto_->PasskeyToTokenAuthData(pkcs11_old_passkey_,
                                         salt_file,
                                         &old_auth_data))
      return false;
    chaps_client.ChangeTokenAuthData(
        token_dir,
        old_auth_data,
        auth_data);
    is_pkcs11_passkey_migration_required_ = false;
    pkcs11_old_passkey_.clear_contents();
  }
  Pkcs11Init pkcs11init;
  int slot_id = 0;
  if (!chaps_client.LoadToken(
      IsolateCredentialManager::GetDefaultIsolateCredential(),
      token_dir,
      auth_data,
      pkcs11init.GetTpmTokenLabelForUser(current_user_->username()),
      &slot_id)) {
    LOG(ERROR) << "Failed to load PKCS #11 token.";
  }
  pkcs11_passkey_.clear_contents();
  return true;
}

void Mount::RemovePkcs11Token() {
  std::string username = current_user_->username();
  FilePath token_dir = homedirs_->GetChapsTokenDir(username);
  chaps::TokenManagerClient chaps_client;
  chaps_client.UnloadToken(
      IsolateCredentialManager::GetDefaultIsolateCredential(),
      token_dir);
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
      st.st_mode & S_ISVTX &&
      st.st_uid == kMountOwnerUid &&
      st.st_gid == kDaemonStoreGid) {
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
  if (!platform_->SetOwnership(root_path.value(), kMountOwnerUid,
                               kDaemonStoreGid)) {
    PLOG(ERROR) << "SetOwnership() failed: " << root_path.value();
    return;
  }
  if (!platform_->SetPermissions(root_path.value(),
                                 S_IRWXU | S_IRWXG | S_ISVTX)) {
    PLOG(ERROR) << "SetPermissions() failed: " << root_path.value();
    return;
  }
  LOG(INFO) << "Migrated (or created) user directory: " << vault_path.c_str();
}

void Mount::CopySkeleton() const {
  CHECK(current_user_);
  FilePath destination = FilePath(GetEphemeralSkeletonPath());
  // For a Mount with a real vault, the skeleton can be safely
  // prepared under /home/.shadow/[hash]/mount/user, but for
  // ephemeral mounts, we use a single location.
  if (IsVaultMounted()) {
    std::string user;
    current_user_->GetObfuscatedUsername(&user);
    destination = FilePath(GetMountedUserHomePath(user));
  } else if (!platform_->IsDirectoryMounted(destination.value())) {
    LOG(ERROR) << "CopySkeleton with no mounted vault or ephemeral path.";
    return;
  }
  RecursiveCopy(destination, FilePath(skel_source_));
}


bool Mount::CacheOldFiles(const std::vector<std::string>& files) const {
  for (unsigned int index = 0; index < files.size(); ++index) {
    FilePath file(files[index]);
    FilePath file_bak(StringPrintf("%s.bak", files[index].c_str()));
    if (platform_->FileExists(file_bak.value())) {
      if (!platform_->DeleteFile(file_bak.value(), false)) {
        return false;
      }
    }
    if (platform_->FileExists(file.value())) {
      if (!platform_->Move(file.value(), file_bak.value())) {
        return false;
      }
    }
  }
  return true;
}

void Mount::RecursiveCopy(const FilePath& destination,
                          const FilePath& source) const {
  scoped_ptr<FileEnumerator> file_enumerator(
      platform_->GetFileEnumerator(source.value(), false,
                                   FileEnumerator::FILES));
  std::string next_path;
  while (!(next_path = file_enumerator->Next()).empty()) {
    FilePath file_name = FilePath(next_path).BaseName();
    FilePath destination_file = destination.Append(file_name);
    if (!platform_->Copy(next_path, destination_file.value()) ||
        !platform_->SetOwnership(destination_file.value(),
                                 default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of destination path: "
                 << destination_file.value().c_str();
    }
  }
  scoped_ptr<FileEnumerator> dir_enumerator(
      platform_->GetFileEnumerator(source.value(), false,
                                   FileEnumerator::DIRECTORIES));
  while (!(next_path = dir_enumerator->Next()).empty()) {
    FilePath dir_name = FilePath(next_path).BaseName();
    FilePath destination_dir = destination.Append(dir_name);
    LOG(INFO) << "RecursiveCopy: " << destination_dir.value();
    if (!platform_->CreateDirectory(destination_dir.value()) ||
        !platform_->SetOwnership(destination_dir.value(),
                                 default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of destination path: "
                 << destination_dir.value().c_str();
    }
    RecursiveCopy(destination_dir, FilePath(next_path));
  }
}

bool Mount::RevertCacheFiles(const std::vector<std::string>& files) const {
  for (unsigned int index = 0; index < files.size(); ++index) {
    FilePath file(files[index]);
    FilePath file_bak(StringPrintf("%s.bak", files[index].c_str()));
    if (platform_->FileExists(file_bak.value())) {
      if (!platform_->Move(file_bak.value(), file.value())) {
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
    if (platform_->FileExists(file_bak.value())) {
      if (!platform_->DeleteFile(file_bak.value(), false)) {
        return false;
      }
    }
  }
  return true;
}

void Mount::GetUserSalt(const Credentials& credentials, bool force,
                        int key_index, SecureBlob* salt) const {
  FilePath path(GetUserSaltFileForUser(
                  credentials.GetObfuscatedUsername(system_salt_),
                  key_index));
  crypto_->GetOrCreateSalt(path, CRYPTOHOME_DEFAULT_SALT_LENGTH, force, salt);
}

bool Mount::EnsurePathComponent(const FilePath& fp, size_t num,
                                uid_t uid, gid_t gid) const {
  std::vector<std::string> path_parts;
  fp.GetComponents(&path_parts);
  FilePath check_path(path_parts[0]);
  for (size_t i = 1; i < num; i++)
    check_path = check_path.Append(path_parts[i]);

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
  return true;
}

bool Mount::EnsureNewUserDirExists(const FilePath& fp, uid_t uid,
                                   gid_t gid) const {
  std::vector<std::string> path_parts;
  if (!EnsureDirHasOwner(fp.DirName(), uid, gid))
    return false;
  return platform_->CreateDirectory(fp.value());
}

bool Mount::EnsureDirHasOwner(const FilePath& fp, uid_t final_uid,
                              gid_t final_gid) const {
  std::vector<std::string> path_parts;
  fp.GetComponents(&path_parts);
  // The path given should be absolute to that its first part is /. This is not
  // actually checked so that relative paths can be used during testing.
  for (size_t i = 2; i <= path_parts.size(); i++) {
    bool last = (i == path_parts.size());
    uid_t uid = last ? final_uid : kMountOwnerUid;
    gid_t gid = last ? final_gid : kMountOwnerGid;
    if (!EnsurePathComponent(fp, i, uid, gid))
      return false;
  }
  return true;
}

bool Mount::EnsureUserMountPoints(const Credentials& credentials) const {
  const std::string username = credentials.username();
  FilePath root_path = chromeos::cryptohome::home::GetRootPath(username);
  FilePath user_path = chromeos::cryptohome::home::GetUserPath(username);
  FilePath temp_path(GetNewUserPath(username));
  if (!EnsureDirHasOwner(root_path, kMountOwnerUid, kMountOwnerGid)) {
    LOG(ERROR) << "Couldn't ensure root path: " << root_path.value();
    return false;
  }
  if (!EnsureDirHasOwner(user_path, default_user_, default_access_group_)) {
    LOG(ERROR) << "Couldn't ensure user path: " << user_path.value();
    return false;
  }
  if (!EnsureNewUserDirExists(temp_path, default_user_, default_group_)) {
    LOG(ERROR) << "Couldn't ensure temp path: " << temp_path.value();
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
  std::vector<int> key_indices;
  if (user.length() && homedirs_->GetVaultKeysets(user, &key_indices)) {
    std::vector<int>::const_iterator iter = key_indices.begin();
    for ( ;iter != key_indices.end(); ++iter) {
      DictionaryValue* keyset_dict = new DictionaryValue();
      if (LoadVaultKeysetForUser(user, *iter, &keyset)) {
        bool tpm = keyset.flags() & SerializedVaultKeyset::TPM_WRAPPED;
        bool scrypt = keyset.flags() & SerializedVaultKeyset::SCRYPT_WRAPPED;
        keyset_dict->SetBoolean("tpm", tpm);
        keyset_dict->SetBoolean("scrypt", scrypt);
        keyset_dict->SetBoolean("ok", true);
      } else {
        keyset_dict->SetBoolean("ok", false);
      }
      if (!ephemeral_mount_ && *iter == current_user_->key_index())
        keyset_dict->SetBoolean("current", true);
      keyset_dict->SetInteger("index", *iter);
      keysets->Append(keyset_dict);
    }
  }
  dv->Set("keysets", keysets);
  dv->SetBoolean("mounted", IsMounted());
  dv->SetString("owner", GetObfuscatedOwner());
  dv->SetBoolean("enterprise", enterprise_owned_);
  return dv;
}

std::string Mount::GetNewUserPath(const std::string& username) const {
  std::string sanitized =
      chromeos::cryptohome::home::SanitizeUserName(username);
  return StringPrintf("/home/chronos/u-%s", sanitized.c_str());
}

bool Mount::MountLegacyHome(const std::string& from, MountError* mount_error) {
  // Multiple mounts can't live on the legacy mountpoint.
  if (platform_->IsDirectoryMounted(kDefaultHomeDir)) {
    LOG(INFO) << "Skipping binding to /home/chronos/user.";
  } else if (!BindForUser(current_user_, from, kDefaultHomeDir)) {
    PLOG(ERROR) << "Bind mount failed: " << from << " -> "
                << kDefaultHomeDir;
    UnmountAllForUser(current_user_);
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  return true;
}

}  // namespace cryptohome
