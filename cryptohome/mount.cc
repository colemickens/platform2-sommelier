// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

#include "cryptohome/mount.h"

#include <errno.h>
#include <sys/stat.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/sha1.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <base/values.h>
#include <chaps/isolate.h>
#include <chaps/token_manager_client.h>
#include <brillo/cryptohome.h>
#include <brillo/secure_blob.h>

#include "cryptohome/boot_lockbox.h"
#include "cryptohome/chaps_client_factory.h"
#include "cryptohome/crypto.h"
#include "cryptohome/cryptohome_common.h"
#include "cryptohome/cryptohome_metrics.h"
#include "cryptohome/cryptolib.h"
#include "cryptohome/dircrypto_data_migrator/migration_helper.h"
#include "cryptohome/dircrypto_util.h"
#include "cryptohome/homedirs.h"
#include "cryptohome/pkcs11_init.h"
#include "cryptohome/platform.h"
#include "cryptohome/tpm.h"
#include "cryptohome/username_passkey.h"
#include "cryptohome/vault_keyset.h"

#include "vault_keyset.pb.h"  // NOLINT(build/include)

using base::FilePath;
using base::StringPrintf;
using chaps::IsolateCredentialManager;
using brillo::cryptohome::home::kGuestUserName;
using brillo::cryptohome::home::GetRootPath;
using brillo::cryptohome::home::GetUserPath;
using brillo::cryptohome::home::IsSanitizedUserName;
using brillo::cryptohome::home::SanitizeUserName;
using brillo::SecureBlob;

namespace cryptohome {

const FilePath::CharType kDefaultHomeDir[] = "/home/chronos/user";
const FilePath::CharType kDefaultShadowRoot[] = "/home/.shadow";
const char kDefaultSharedUser[] = "chronos";
const char kChapsUserName[] = "chaps";
const char kDefaultSharedAccessGroup[] = "chronos-access";
const FilePath::CharType kDefaultSkeletonSource[] = "/etc/skel";
const uid_t kMountOwnerUid = 0;
const gid_t kMountOwnerGid = 0;
// TODO(fes): Remove once UI for BWSI switches to MountGuest()
const char kIncognitoUser[] = "incognito";
// Tracked directories - special sub-directories of the cryptohome
// vault, that are visible even if not mounted. Contents is still encrypted.
const FilePath::CharType kVaultDir[] = "vault";
const FilePath::CharType kCacheDir[] = "Cache";
const FilePath::CharType kDownloadsDir[] = "Downloads";
const FilePath::CharType kGCacheDir[] = "GCache";
const FilePath::CharType kGCacheVersionDir[] = "v1";
const FilePath::CharType kGCacheBlobsDir[] = "blobs";
const FilePath::CharType kGCacheTmpDir[] = "tmp";
const char kUserHomeSuffix[] = "user";
const char kRootHomeSuffix[] = "root";
const FilePath::CharType kMountDir[] = "mount";
const FilePath::CharType kTemporaryMountDir[] = "temporary_mount";
const FilePath::CharType kSkeletonDir[] = "skeleton";
const FilePath::CharType kKeyFile[] = "master";
const int kKeyFileMax = 100;  // master.0 ... master.99
const mode_t kKeyFilePermissions = 0600;
const FilePath::CharType kKeyLegacyPrefix[] = "legacy-";
const FilePath::CharType kEphemeralDir[] = "ephemeralfs";
const char kEphemeralMountType[] = "tmpfs";
const FilePath::CharType kGuestMountPath[] = "guestfs";
const char kEphemeralMountPerms[] = "mode=0700";

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
                                          const FilePath& src,
                                          const FilePath& dest)
  : mount_(mount), src_(src), dest_(dest) {
}

Mount::ScopedMountPoint::~ScopedMountPoint() {
  if (mount_->platform_->IsDirectoryMounted(dest_)) {
    mount_->ForceUnmount(src_, dest_);
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
      crypto_(NULL),
      default_homedirs_(new HomeDirs()),
      homedirs_(default_homedirs_.get()),
      use_tpm_(true),
      default_current_user_(new UserSession()),
      current_user_(default_current_user_.get()),
      user_timestamp_cache_(NULL),
      enterprise_owned_(false),
      pkcs11_state_(kUninitialized),
      is_pkcs11_passkey_migration_required_(false),
      dircrypto_key_id_(dircrypto::kInvalidKeySerial),
      legacy_mount_(true),
      mount_type_(MountType::NONE),
      default_chaps_client_factory_(new ChapsClientFactory()),
      chaps_client_factory_(default_chaps_client_factory_.get()),
      boot_lockbox_(NULL) {
}

Mount::~Mount() {
  if (IsMounted())
    UnmountCryptohome();
}

bool Mount::Init(Platform* platform, Crypto* crypto,
                 UserOldestActivityTimestampCache *cache) {
  platform_ = platform;
  crypto_ = crypto;
  user_timestamp_cache_ = cache;

  bool result = true;

  homedirs_->set_platform(platform_);
  homedirs_->set_shadow_root(FilePath(shadow_root_));
  homedirs_->set_enterprise_owned(enterprise_owned_);

  // Make sure both we and |homedirs_| have a proper device policy object.
  EnsureDevicePolicyLoaded(false);
  homedirs_->set_policy_provider(policy_provider_.get());
  if (!homedirs_->Init(platform, crypto, user_timestamp_cache_))
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

  int original_mask = platform_->SetMask(kDefaultUmask);
  // Create the shadow root if it doesn't exist
  if (!platform_->DirectoryExists(shadow_root_)) {
    platform_->CreateDirectory(shadow_root_);
  }

  if (use_tpm_ && !boot_lockbox_) {
    default_boot_lockbox_.reset(
        new BootLockbox(Tpm::GetSingleton(), platform_, crypto_));
    boot_lockbox_ = default_boot_lockbox_.get();
  }

  // One-time load of the global system salt (used in generating username
  // hashes)
  FilePath system_salt_file = shadow_root_.Append("salt");
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
                             const MountArgs& mount_args,
                             bool* created) {
  // If the user has an old-style cryptohome, delete it
  FilePath old_image_path = GetUserDirectory(credentials).Append("image");
  if (platform_->FileExists(old_image_path)) {
    platform_->DeleteFile(GetUserDirectory(credentials), true);
  }
  if (!EnsureUserMountPoints(credentials)) {
    return false;
  }
  // Now check for the presence of a cryptohome.
  if (DoesCryptohomeExist(credentials)) {
    // Now check for the presence of a vault directory.
    FilePath vault_path = GetUserVaultPath(
        credentials.GetObfuscatedUsername(system_salt_));
    if (platform_->DirectoryExists(vault_path)) {
      if (mount_args.to_migrate_from_ecryptfs) {
        // When migrating, set the mount_type_ to dircrypto even if there is an
        // eCryptfs vault.
        mount_type_ = MountType::DIR_CRYPTO;
      } else {
        mount_type_ = MountType::ECRYPTFS;
      }
    } else {
      if (mount_args.to_migrate_from_ecryptfs) {
        LOG(ERROR) << "No eCryptfs vault to migrate.";
        return false;
      } else {
        mount_type_ = MountType::DIR_CRYPTO;
      }
    }
    *created = false;
    return true;
  }
  // Create the cryptohome from scratch.
  // If the kernel supports it, steer toward ext4 crypto.
  if (mount_args.create_as_ecryptfs) {
    mount_type_ = MountType::ECRYPTFS;
  } else {
    dircrypto::KeyState state = platform_->GetDirCryptoKeyState(shadow_root_);
    switch (state) {
      case dircrypto::KeyState::UNKNOWN:
      case dircrypto::KeyState::ENCRYPTED:
        LOG(ERROR) << "Unexpected state " << state;
        return false;
      case dircrypto::KeyState::NOT_SUPPORTED:
        mount_type_ = MountType::ECRYPTFS;
        break;
      case dircrypto::KeyState::NO_KEY:
        mount_type_ = MountType::DIR_CRYPTO;
        break;
    }
  }
  *created = CreateCryptohome(credentials);
  return *created;
}

bool Mount::DoesCryptohomeExist(const Credentials& credentials) const {
  return DoesEcryptfsCryptohomeExist(credentials) ||
      DoesDircryptoCryptohomeExist(credentials);
}

bool Mount::DoesEcryptfsCryptohomeExist(const Credentials& credentials) const {
  // Check for the presence of a vault directory for ecryptfs.
  return platform_->DirectoryExists(GetUserVaultPath(
      credentials.GetObfuscatedUsername(system_salt_)));
}

bool Mount::DoesDircryptoCryptohomeExist(const Credentials& credentials) const {
  // Check for the presence of an encrypted mount directory for dircrypto.
  FilePath mount_path = GetUserMountDirectory(
      credentials.GetObfuscatedUsername(system_salt_));
  return platform_->DirectoryExists(mount_path) &&
      platform_->GetDirCryptoKeyState(mount_path) ==
      dircrypto::KeyState::ENCRYPTED;
}

bool Mount::MountCryptohome(const Credentials& credentials,
                            const Mount::MountArgs& mount_args,
                            MountError* mount_error) {
  CHECK(boot_lockbox_ || !use_tpm_);
  if (boot_lockbox_ && !boot_lockbox_->FinalizeBoot()) {
    LOG(WARNING) << "Failed to finalize boot lockbox.";
  }

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

bool Mount::AddEcryptfsAuthToken(const VaultKeyset& vault_keyset,
                                 std::string* key_signature,
                                 std::string* filename_key_signature) const {
  // Add the File Encryption key (FEK) from the vault keyset.  This is the key
  // that is used to encrypt the file contents when it is persisted to the lower
  // filesystem by eCryptfs.
  *key_signature = CryptoLib::BlobToHex(vault_keyset.fek_sig());
  if (!platform_->AddEcryptfsAuthToken(
        vault_keyset.fek(), *key_signature,
        vault_keyset.fek_salt())) {
    LOG(ERROR) << "Couldn't add ecryptfs key to keyring";
    return false;
  }

  // Add the File Name Encryption Key (FNEK) from the vault keyset.  This is the
  // key that is used to encrypt the file name when it is persisted to the lower
  // filesystem by eCryptfs.
  *filename_key_signature = CryptoLib::BlobToHex(vault_keyset.fnek_sig());
  if (!platform_->AddEcryptfsAuthToken(
        vault_keyset.fnek(), *filename_key_signature,
        vault_keyset.fnek_salt())) {
    LOG(ERROR) << "Couldn't add ecryptfs filename encryption key to keyring";
    return false;
  }

  return true;
}

bool Mount::MountCryptohomeInner(const Credentials& credentials,
                                 const Mount::MountArgs& mount_args,
                                 bool recreate_decrypt_fatal,
                                 MountError* mount_error) {
  current_user_->Reset();

  std::string username = credentials.username();
  if (username.compare(kIncognitoUser) == 0) {
    // TODO(fes): Have guest set error conditions?
    *mount_error = MOUNT_ERROR_NONE;
    return MountGuestCryptohome();
  }

  ReloadDevicePolicy();
  bool ephemeral_users = AreEphemeralUsersEnabled();
  const std::string obfuscated_owner = GetObfuscatedOwner();
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
    LOG(ERROR) << "Asked to mount nonexistent user";
    *mount_error = MOUNT_ERROR_USER_DOES_NOT_EXIST;
    return false;
  }

  bool created = false;
  if (!EnsureCryptohome(credentials, mount_args, &created)) {
    LOG(ERROR) << "Error creating cryptohome.";
    *mount_error = MOUNT_ERROR_FATAL;
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
    *mount_error = local_mount_error;
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
      if (local_result) {
        *mount_error = MOUNT_ERROR_RECREATED;
      }
      return local_result;
    }
    return false;
  }

  if (!serialized.has_wrapped_chaps_key()) {
    is_pkcs11_passkey_migration_required_ = true;
    vault_keyset.CreateRandomChapsKey();
    ReEncryptVaultKeyset(credentials, vault_keyset, index, &serialized);
  }

  SecureBlob local_chaps_key(vault_keyset.chaps_key().begin(),
                             vault_keyset.chaps_key().end());
  pkcs11_token_auth_data_.swap(local_chaps_key);
  platform_->ClearUserKeyring();

  // Before we use the matching keyset, make sure it isn't being misused.
  // Note, privileges don't protect against information leakage, they are
  // just software/DAC policy enforcement mechanisms.
  //
  // In the future we may provide some assurance by wrapping privileges
  // with the wrapped_key, but that is still of limited benefit.
  if (serialized.has_key_data() &&  // legacy keys are full privs
      !serialized.key_data().privileges().mount()) {
    // TODO(wad): Convert to CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED
    // TODO(wad): Expose the safe-printable label rather than the Chrome
    //            supplied one for log output.
    LOG(INFO) << "Mount attempt with unprivileged key";
    *mount_error = MOUNT_ERROR_KEY_FAILURE;
    return false;
  }

  // Checks whether migration from ecryptfs to dircrypto is needed, and returns
  // an error when necessary. Do this after the check by DecryptVaultKeyset,
  // because a correct credential is required before switching to migration UI.
  if (DoesEcryptfsCryptohomeExist(credentials) &&
      DoesDircryptoCryptohomeExist(credentials) &&
      !mount_args.to_migrate_from_ecryptfs) {
    // If both types of home directory existed, it implies that the migration
    // attempt was aborted in the middle before doing clean up.
    LOG(INFO) << "Mount failed because both ecryptfs and dircrypto home "
        "directory is found. Need to resume and finish migration first.";
    *mount_error = MOUNT_ERROR_PREVIOUS_MIGRATION_INCOMPLETE;
    return false;
  }

  if (mount_type_ == MountType::ECRYPTFS && mount_args.force_dircrypto) {
    // If dircrypto is forced, it's an error to mount ecryptfs home.
    LOG(INFO) << "Mount attempt with force_dircrypto on ecryptfs.";
    *mount_error = MOUNT_ERROR_OLD_ENCRYPTION;
    return false;
  }

  if (!platform_->SetupProcessKeyring()) {
    LOG(INFO) << "Failed to set up a process keyring.";
    *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }
  // When migrating, mount both eCryptfs and dircrypto.
  const bool should_mount_ecryptfs = mount_type_ == MountType::ECRYPTFS ||
      mount_args.to_migrate_from_ecryptfs;
  const bool should_mount_dircrypto = mount_type_ == MountType::DIR_CRYPTO;
  if (!should_mount_ecryptfs && !should_mount_dircrypto) {
    NOTREACHED() << "Unexpected mount type " << mount_type_;
    *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }
  std::string ecryptfs_options;
  if (should_mount_ecryptfs) {
    // Add the decrypted key to the keyring so that ecryptfs can use it.
    std::string key_signature, fnek_signature;
    if (!AddEcryptfsAuthToken(vault_keyset, &key_signature,
                              &fnek_signature)) {
      LOG(INFO) << "Cryptohome mount failed because of keyring failure.";
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }
    // Specify the ecryptfs options for mounting the user's cryptohome.
    ecryptfs_options = StringPrintf(
        "ecryptfs_cipher=aes"
        ",ecryptfs_key_bytes=%d"
        ",ecryptfs_fnek_sig=%s"
        ",ecryptfs_sig=%s"
        ",ecryptfs_unlink_sigs",
        kDefaultEcryptfsKeySize,
        fnek_signature.c_str(),
        key_signature.c_str());
  }
  if (should_mount_dircrypto) {
    LOG_IF(WARNING, dircrypto_key_id_ != dircrypto::kInvalidKeySerial)
        << "Already mounting with key " << dircrypto_key_id_;
    if (!platform_->AddDirCryptoKeyToKeyring(
            vault_keyset.fek(), vault_keyset.fek_sig(), &dircrypto_key_id_)) {
      LOG(INFO) << "Error adding dircrypto key.";
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }
  }

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

  std::string obfuscated_username =
    credentials.GetObfuscatedUsername(system_salt_);
  FilePath vault_path = GetUserVaultPath(obfuscated_username);

  mount_point_ = GetUserMountDirectory(obfuscated_username);
  if (!platform_->CreateDirectory(mount_point_)) {
    PLOG(ERROR) << "Directory creation failed for " << mount_point_.value();
    UnmountAll();
    *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }
  if (mount_args.to_migrate_from_ecryptfs) {
    FilePath temporary_mount_point =
        GetUserTemporaryMountDirectory(obfuscated_username);
    if (!platform_->CreateDirectory(temporary_mount_point)) {
      PLOG(ERROR) << "Directory creation failed for "
                  << temporary_mount_point.value();
      UnmountAll();
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }
  }

  // Since Service::Mount cleans up stale mounts, we should only reach
  // this point if someone attempts to re-mount an in-use mount point.
  if (platform_->IsDirectoryMounted(mount_point_)) {
    LOG(ERROR) << "Mount point is busy: " << mount_point_.value()
               << " for " << vault_path.value();
    UnmountAll();
    *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }
  if (should_mount_ecryptfs) {
    // Create vault_path/user as a passthrough directory, move all the
    // (encrypted) contents of vault_path into vault_path/user, create
    // vault_path/root.
    MigrateToUserHome(vault_path);

    FilePath dest = mount_args.to_migrate_from_ecryptfs ?
        GetUserTemporaryMountDirectory(obfuscated_username) : mount_point_;
    if (!RememberMount(vault_path, dest, "ecryptfs", ecryptfs_options)) {
      PLOG(ERROR) << "Cryptohome mount failed for vault "
                  << vault_path.value();
      UnmountAll();
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }
  }
  if (should_mount_dircrypto) {
    if (!platform_->SetDirCryptoKey(mount_point_, vault_keyset.fek_sig())) {
      LOG(ERROR) << "Failed to set directory encryption policy "
                 << mount_point_.value();
      UnmountAll();
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }
    // Create user & root directories.
    MigrateToUserHome(mount_point_);
  }

  // Set the current user here so we can rely on it in the helpers..
  // On failure, they will linger, but should be reset on a new MountCryptohome
  // request.
  current_user_->SetUser(credentials);
  current_user_->set_key_index(index);
  if (serialized.has_key_data()) {
    current_user_->set_key_data(serialized.key_data());
  }

  // Move the tracked subdirectories from mount_point_/user to vault_path
  // as passthrough directories.
  CreateTrackedSubdirectories(credentials, created);

  FilePath user_home = GetMountedUserHomePath(obfuscated_username);
  if (created)
    CopySkeleton(user_home);

  if (!SetupGroupAccess(FilePath(user_home))) {
    UnmountAll();
    *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }

  // When migrating, it's better to avoid exposing the new ext4 crypto dir.
  if (!mount_args.to_migrate_from_ecryptfs) {
    if (legacy_mount_)
      MountLegacyHome(user_home, mount_error);

    FilePath user_multi_home = GetUserPath(username);
    if (!RememberBind(user_home, user_multi_home)) {
      PLOG(ERROR) << "Bind mount failed: " << user_home.value() << " -> "
                  << user_multi_home.value();
      UnmountAll();
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }

    // Mount /home/chronos/u<s_h_o_u>.
    FilePath multi_home = GetNewUserPath(username);
    if (!RememberBind(user_home, multi_home)) {
      PLOG(ERROR) << "Bind mount failed: " << user_home.value() << " -> "
                  << multi_home.value();
      UnmountAll();
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }

    FilePath root_home = GetMountedRootHomePath(obfuscated_username);
    FilePath root_multi_home = GetRootPath(username);
    if (!RememberBind(root_home, root_multi_home)) {
      PLOG(ERROR) << "Bind mount failed: " << root_home.value() << " -> "
                  << root_multi_home.value();
      UnmountAll();
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }
  }

  // TODO(ellyjones): Expose the path to the root directory over dbus for use by
  // daemons. We may also want to bind-mount it somewhere stable.

  *mount_error = MOUNT_ERROR_NONE;

  if (is_pkcs11_passkey_migration_required_) {
    credentials.GetPasskey(&legacy_pkcs11_passkey_);
  }
  return true;
}

bool Mount::MountEphemeralCryptohome(const Credentials& credentials) {
  const std::string username = credentials.username();
  FilePath path = GetUserEphemeralPath(credentials.GetObfuscatedUsername(
      system_salt_));
  const FilePath user_multi_home = GetUserPath(username);
  const FilePath root_multi_home = GetRootPath(username);

  // If we're mounting as a guest, as source use just "guestfs" instead of an
  // actual path. We don't want the guest cryptohome to persist even between
  // logins during the same boot.
  if (credentials.username() == kGuestUserName)
    path = FilePath(kGuestMountPath);

  if (!EnsureUserMountPoints(credentials))
    return false;
  if (!SetUpEphemeralCryptohome(path, user_multi_home))
    return false;
  if (!RememberMount(path,
                    root_multi_home,
                    kEphemeralMountType,
                    kEphemeralMountPerms)) {
    LOG(ERROR) << "Mount of ephemeral root home at " << root_multi_home.value()
               << "failed: " << errno;
    UnmountAll();
    return false;
  }

  if (legacy_mount_)
    MountLegacyHome(user_multi_home, NULL);

  FilePath multi_home = GetNewUserPath(username);
  if (!RememberBind(user_multi_home, multi_home)) {
    PLOG(ERROR) << "Bind mount failed: " << user_multi_home.value() << " -> "
                << multi_home.value();
    UnmountAll();
    return false;
  }
  mount_type_ = MountType::EPHEMERAL;
  return true;
}

bool Mount::SetUpEphemeralCryptohome(const FilePath& source_path,
                                     const FilePath& home_dir) {
  // First, build up the home dir at a mount point not accessible to chronos.
  // This helps to avoid chown race conditions.
  const FilePath ephemeral_skeleton_path = GetEphemeralSkeletonPath();
  if (!platform_->CreateDirectory(ephemeral_skeleton_path)) {
    LOG(ERROR) << "Failed to create " << ephemeral_skeleton_path.value() << ": "
               << errno;
    return false;
  }
  // Note! This mount point does not show up in the MountStack.
  // TODO(wad) check for existing mount first.
  if (!platform_->Mount(source_path,
                        ephemeral_skeleton_path,
                        kEphemeralMountType,
                        kEphemeralMountPerms)) {
    LOG(ERROR) << "Mount of ephemeral skeleton at "
               << ephemeral_skeleton_path.value()
               << "failed: " << errno;
    return false;
  }
  // Whatever happens, we want to unmount the tmpfs used to build the skeleton
  // home directory.
  ScopedMountPoint scoped_skeleton_mount(this, source_path,
                                         ephemeral_skeleton_path);
  CopySkeleton(ephemeral_skeleton_path);

  // Create the Downloads directory if it does not exist so that it can later be
  // made group accessible when SetupGroupAccess() is called.
  FilePath downloads_path =
      FilePath(ephemeral_skeleton_path).Append(kDownloadsDir);
  if (!platform_->DirectoryExists(downloads_path)) {
    if (!platform_->CreateDirectory(downloads_path) ||
        !platform_->SetOwnership(
            downloads_path, default_user_, default_group_, true)) {
      LOG(ERROR) << "Couldn't create user Downloads directory: "
                 << downloads_path.value();
      return false;
    }
  }

  if (!platform_->SetOwnership(ephemeral_skeleton_path,
                               default_user_,
                               default_access_group_,
                               true)) {
    LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
               << default_access_group_ << ") of path: "
               << ephemeral_skeleton_path.value();
    return false;
  }

  if (!SetupGroupAccess(FilePath(ephemeral_skeleton_path)))
    return false;

  if (!RememberBind(ephemeral_skeleton_path, home_dir)) {
    LOG(ERROR) << "Bind mount of ephemeral user home from "
               << ephemeral_skeleton_path.value() << " to "
               << home_dir.value() << " failed: " << errno;
    return false;
  }
  return true;
}

bool Mount::RememberMount(const FilePath& src,
                          const FilePath& dest, const std::string& type,
                          const std::string& options) {
  if (platform_->Mount(src, dest, type, options)) {
    mounts_.Push(src, dest);
    return true;
  }
  return false;
}

bool Mount::RememberBind(const FilePath& src,
                         const FilePath& dest) {
  if (platform_->Bind(src, dest)) {
    mounts_.Push(src, dest);
    return true;
  }
  return false;
}

void Mount::UnmountAll() {
  FilePath src, dest;
  while (mounts_.Pop(&src, &dest)) {
    ForceUnmount(src, dest);
  }

  // Invalidate dircrypto key to make directory contents inaccessible.
  if (dircrypto_key_id_ != dircrypto::kInvalidKeySerial) {
    platform_->InvalidateDirCryptoKey(dircrypto_key_id_);
    dircrypto_key_id_ = dircrypto::kInvalidKeySerial;
  }
}

void Mount::ForceUnmount(const FilePath& src, const FilePath& dest) {
  // Try an immediate unmount
  bool was_busy;
  if (!platform_->Unmount(dest, false, &was_busy)) {
    LOG(ERROR) << "Couldn't unmount vault immediately, was_busy = " << was_busy;
    if (was_busy) {
      std::vector<ProcessInformation> processes;
      platform_->GetProcessesWithOpenFiles(dest, &processes);
      for (const auto& proc : processes) {
        LOG(ERROR) << "Process " << proc.get_process_id()
                   << " had open files.  Command line: "
                   << proc.GetCommandLine();
        if (proc.get_cwd().length()) {
          LOG(ERROR) << "  (" << proc.get_process_id() << ") CWD: "
                     << proc.get_cwd();
        }
        for (const auto& file : proc.get_open_files()) {
          LOG(ERROR) << "  (" << proc.get_process_id() << ") Open File: "
                     << file.value();
        }
      }
    }
    // Failed to unmount immediately, do a lazy unmount.  If |was_busy| we also
    // want to sync before the unmount to help prevent data loss.
    if (was_busy)
      platform_->SyncDirectory(dest);
    platform_->LazyUnmount(dest);
    platform_->SyncDirectory(src);
  }
}

bool Mount::UnmountCryptohome() {
  UnmountAll();
  ReloadDevicePolicy();
  if (AreEphemeralUsersEnabled())
    homedirs_->RemoveNonOwnerCryptohomes();
  else
    UpdateCurrentUserActivityTimestamp(0);

  RemovePkcs11Token();
  current_user_->Reset();
  mount_type_ = MountType::NONE;

  platform_->ClearUserKeyring();

  return true;
}

bool Mount::IsMounted() const {
  return mounts_.size() != 0;
}

bool Mount::IsNonEphemeralMounted() const {
  return IsMounted() && mount_type_ != MountType::EPHEMERAL;
}

bool Mount::OwnsMountPoint(const FilePath& path) const {
  return mounts_.ContainsDest(path);
}

bool Mount::CreateCryptohome(const Credentials& credentials) const {
  int original_mask = platform_->SetMask(kDefaultUmask);

  // Create the user's entry in the shadow root
  FilePath user_dir(GetUserDirectory(credentials));
  platform_->CreateDirectory(user_dir);

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
  // Merge in the key data from credentials using the label() as
  // the existence test. (All new-format calls must populate the
  // label on creation.)
  if (!credentials.key_data().label().empty()) {
    *serialized.mutable_key_data() = credentials.key_data();
  }

  // TODO(wad) move to storage by label-derivative and not number.
  if (!StoreVaultKeysetForUser(
       credentials.GetObfuscatedUsername(system_salt_),
       0,  // first key
       serialized)) {
    platform_->SetMask(original_mask);
    LOG(ERROR) << "Failed to store vault keyset for new user";
    return false;
  }

  if (mount_type_ == MountType::ECRYPTFS) {
    // Create the user's vault.
    FilePath vault_path = GetUserVaultPath(
        credentials.GetObfuscatedUsername(system_salt_));
    if (!platform_->CreateDirectory(vault_path)) {
      LOG(ERROR) << "Couldn't create vault path: " << vault_path.value();
      platform_->SetMask(original_mask);
      return false;
    }
  }

  // Restore the umask
  platform_->SetMask(original_mask);
  return true;
}

// static
std::vector<FilePath> Mount::GetTrackedSubdirectories() {
  return std::vector<FilePath>{
    FilePath(kRootHomeSuffix),
    FilePath(kUserHomeSuffix),
    FilePath(kUserHomeSuffix).Append(kCacheDir),
    FilePath(kUserHomeSuffix).Append(kDownloadsDir),
    FilePath(kUserHomeSuffix).Append(kGCacheDir),
    FilePath(kUserHomeSuffix).Append(kGCacheDir)
                             .Append(kGCacheVersionDir),
    FilePath(kUserHomeSuffix).Append(kGCacheDir)
                             .Append(kGCacheVersionDir)
                             .Append(kGCacheBlobsDir),
    FilePath(kUserHomeSuffix).Append(kGCacheDir)
                             .Append(kGCacheVersionDir)
                             .Append(kGCacheTmpDir),
  };
}

bool Mount::CreateTrackedSubdirectories(const Credentials& credentials,
                                        bool is_new) const {
  ScopedUmask scoped_umask(platform_, kDefaultUmask);

  // Add the subdirectories if they do not exist.
  const std::string obfuscated_username =
      credentials.GetObfuscatedUsername(system_salt_);
  const FilePath dest_dir(mount_type_ == MountType::ECRYPTFS ?
                          GetUserVaultPath(obfuscated_username) :
                          GetUserMountDirectory(obfuscated_username));
  if (!platform_->DirectoryExists(dest_dir)) {
     LOG(ERROR) << "Can't create tracked subdirectories for a missing user.";
     return false;
  }

  const FilePath user_home(GetMountedUserHomePath(obfuscated_username));

  // The call is allowed to partially fail if directory creation fails, but we
  // want to have as many of the specified tracked directories created as
  // possible.
  bool result = true;
  for (const auto& tracked_dir : GetTrackedSubdirectories()) {
    const FilePath tracked_dir_path = dest_dir.Append(tracked_dir);
    if (mount_type_ == MountType::ECRYPTFS) {
      const FilePath userside_dir = user_home.Append(tracked_dir);
      // If non-pass-through dir with the same name existed - delete it
      // to prevent duplication.
      if (!is_new && platform_->DirectoryExists(userside_dir) &&
          !platform_->DirectoryExists(tracked_dir_path)) {
        platform_->DeleteFile(userside_dir, true);
      }
    }

    // Create pass-through directory.
    if (!platform_->DirectoryExists(tracked_dir_path)) {
      LOG(INFO) << "Creating pass-through directories "
                << tracked_dir_path.value();
      platform_->CreateDirectory(tracked_dir_path);
      if (!platform_->SetOwnership(
              tracked_dir_path, default_user_, default_group_, true)) {
        LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                   << default_group_ << ") of tracked directory path: "
                   << tracked_dir_path.value();
        platform_->DeleteFile(tracked_dir_path, true);
        result = false;
        continue;
      }
    }
    if (mount_type_ == MountType::DIR_CRYPTO) {
      // Set xattr to make this directory trackable.
      std::string name = tracked_dir_path.BaseName().value();
      if (!platform_->SetExtendedFileAttribute(
              tracked_dir_path,
              kTrackedDirectoryNameAttribute,
              name.data(),
              name.length())) {
        PLOG(ERROR) << "Unable to set xattr " << tracked_dir_path.value();
        result = false;
        continue;
      }
    }
  }
  return result;
}

bool Mount::UpdateCurrentUserActivityTimestamp(int time_shift_sec) {
  std::string obfuscated_username;
  current_user_->GetObfuscatedUsername(&obfuscated_username);
  if (!obfuscated_username.empty() && mount_type_ != MountType::EPHEMERAL) {
    SerializedVaultKeyset serialized;
    // TODO(wad) Start using current_user_'s key_data label when
    //           it is defined.
    LoadVaultKeysetForUser(obfuscated_username, current_user_->key_index(),
                           &serialized);
    base::Time timestamp = platform_->GetCurrentTime();
    if (time_shift_sec > 0)
      timestamp -= base::TimeDelta::FromSeconds(time_shift_sec);
    serialized.set_last_activity_timestamp(timestamp.ToInternalValue());
    // Only update the key in use.
    StoreVaultKeysetForUser(obfuscated_username, current_user_->key_index(),
                            serialized);
    if (user_timestamp_cache_->initialized()) {
      user_timestamp_cache_->UpdateExistingUser(
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
  for (const auto& accessible : kGroupAccessiblePaths) {
    if (!platform_->FileExists(accessible.path) &&
        accessible.optional)
      continue;

    if (!platform_->SetGroupAccessible(accessible.path,
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

bool Mount::LoadVaultKeysetForUser(const std::string& obfuscated_username,
                                   int index,
                                   SerializedVaultKeyset* serialized) const {
  if (index < 0 || index > kKeyFileMax) {
    LOG(ERROR) << "Attempted to load an invalid key index: " << index;
    return false;
  }
  // Load the encrypted keyset
  FilePath user_key_file = GetUserLegacyKeyFileForUser(obfuscated_username,
                                                          index);
  if (!platform_->FileExists(user_key_file)) {
    return false;
  }
  SecureBlob cipher_text;
  if (!platform_->ReadFile(user_key_file, &cipher_text)) {
    LOG(ERROR) << "Failed to read keyset file for user " << obfuscated_username;
    return false;
  }
  if (!serialized->ParseFromArray(cipher_text.data(), cipher_text.size())) {
    LOG(ERROR) << "Failed to parse keyset for user " << obfuscated_username;
    return false;
  }
  return true;
}

bool Mount::StoreVaultKeysetForUser(
    const std::string& obfuscated_username,
    int index,
    const SerializedVaultKeyset& serialized) const {
  if (index < 0 || index > kKeyFileMax) {
    LOG(ERROR) << "Attempted to store an invalid key index: " << index;
    return false;
  }
  SecureBlob final_blob(serialized.ByteSize());
  serialized.SerializeWithCachedSizesToArray(
      static_cast<google::protobuf::uint8*>(final_blob.data()));
  return platform_->WriteFileAtomicDurable(
      GetUserLegacyKeyFileForUser(obfuscated_username, index),
      final_blob,
      kKeyFilePermissions);
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
  for (auto key_index : key_indices) {
    // Load the encrypted keyset
    if (!LoadVaultKeysetForUser(obfuscated_username, key_index, serialized)) {
      LOG(ERROR) << "Could not parse keyset " << key_index
                 << " for " << obfuscated_username;
      continue;
    }

    // If a specific key was requested by label, then check if the
    // label matches or if the key does not have a label, use the
    // legacy-key translation from index to label.
    //
    // Label-less requests will still iterate over all keys.
    if (!credentials.key_data().label().empty()) {
      // If we're searching by label, don't let a no-key-found become
      // MOUNT_ERROR_FATAL.  In the past, no parseable key was a fatal
      // error.  Just treat it like an invalid key.  This allows for
      // multiple per-label requests then a wildcard, worst case, before
      // the Cryptohome is removed.
      *error = MOUNT_ERROR_KEY_FAILURE;
      if (serialized->has_key_data()) {
        if (credentials.key_data().label() != serialized->key_data().label())
          continue;
      } else {
        if (credentials.key_data().label() !=
            StringPrintf("%s%d", kKeyLegacyPrefix, key_index))
          continue;
      }
    }

    // Attempt decrypt the master key with the passkey
    crypt_flags = 0;
    crypto_error = Crypto::CE_NONE;
    if (crypto_->DecryptVaultKeyset(*serialized, passkey, &crypt_flags,
                                    &crypto_error, vault_keyset)) {
      // Success!
      *error = MOUNT_ERROR_NONE;
      *index = key_index;
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
  //                      1   2   3   4   5   6   7   8   9  10  11  12  13
  // use_tpm              -   -   -   X   X   X   X   X   X   -   -   -   *
  //
  // fallback_to_scrypt   -   -   -   -   -   -   X   X   X   X   X   X   *
  //
  // tpm_wrapped          -   X   -   -   X   -   -   X   -   -   X   -   X
  //
  // scrypt_wrapped       -   -   X   -   -   X   -   -   X   -   -   X   X
  //
  // migrate              N   Y   Y   Y   N   Y   Y   N   Y   Y   Y   N   Y
  bool tpm_wrapped =
      (crypt_flags & SerializedVaultKeyset::TPM_WRAPPED) != 0;
  bool scrypt_wrapped =
      (crypt_flags & SerializedVaultKeyset::SCRYPT_WRAPPED) != 0;
  bool should_tpm = (crypto_->has_tpm() && use_tpm_ &&
                     crypto_->is_cryptohome_key_loaded());
  bool should_scrypt = true;
  do {
    // If the keyset was TPM-wrapped, but there was no public key hash,
    // always re-save.  Otherwise, check the table.
    if (crypto_error != Crypto::CE_NO_PUBLIC_KEY_HASH) {
      if (tpm_wrapped && should_tpm && !scrypt_wrapped)
        break;  // 5, 8
      if (scrypt_wrapped && should_scrypt && !should_tpm && !tpm_wrapped)
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
  CryptoLib::GetSecureRandom(salt.data(), salt.size());

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
  std::vector<FilePath> files(2);
  files[0] = GetUserSaltFileForUser(obfuscated_username, key_index);
  files[1] = GetUserLegacyKeyFileForUser(obfuscated_username, key_index);
  if (!CacheOldFiles(files)) {
    LOG(ERROR) << "Couldn't cache old key material.";
    return false;
  }
  if (!AddVaultKeyset(credentials, vault_keyset, serialized)) {
    LOG(ERROR) << "Couldn't add keyset.";
    RevertCacheFiles(files);
    return false;
  }

  // Note that existing legacy keysets are not automatically annotated.
  // All _new_ interfaces that support KeyData will implicitly translate
  // master.<index> to label=<kKeyLegacyFormat,index> for checking on
  // label uniqueness.  This means that we will still be able to use the
  // lack of KeyData in the future as input to migration.
  if (!StoreVaultKeysetForUser(
        credentials.GetObfuscatedUsername(system_salt_),
        key_index, *serialized)) {
    LOG(ERROR) << "Write to master key failed";
    RevertCacheFiles(files);
    return false;
  }
  DeleteCacheFiles(files);
  return true;
}

bool Mount::MountGuestCryptohome() {
  CHECK(boot_lockbox_ || !use_tpm_);
  if (boot_lockbox_ && !boot_lockbox_->FinalizeBoot()) {
    LOG(WARNING) << "Failed to finalize boot lockbox.";
  }

  std::string guest = kGuestUserName;
  UsernamePasskey guest_creds(guest.c_str(), brillo::Blob(0));
  current_user_->Reset();
  return MountEphemeralCryptohome(guest_creds);
}

FilePath Mount::GetUserDirectory(
    const Credentials& credentials) const {
  return GetUserDirectoryForUser(
      credentials.GetObfuscatedUsername(system_salt_));
}

FilePath Mount::GetUserDirectoryForUser(
    const std::string& obfuscated_username) const {
  return shadow_root_.Append(obfuscated_username);
}

FilePath Mount::GetUserSaltFileForUser(
    const std::string& obfuscated_username, int index) const {
  return GetUserLegacyKeyFileForUser(obfuscated_username, index)
      .AddExtension("salt");
}

FilePath Mount::GetUserLegacyKeyFileForUser(
    const std::string& obfuscated_username, int index) const {
  DCHECK(index < kKeyFileMax && index >= 0);
  return shadow_root_.Append(obfuscated_username)
                     .Append(kKeyFile)
                     .AddExtension(base::IntToString(index));
}

// This is the new planned format for keyfile storage.
FilePath Mount::GetUserKeyFileForUser(
    const std::string& obfuscated_username, const std::string& label) const {
  DCHECK(!label.empty());
  // SHA1 is not for any other purpose than to provide a reasonably
  // collision-resistant, fixed length, path-safe file suffix.
  std::string digest = base::SHA1HashString(label);
  std::string safe_label = base::HexEncode(digest.c_str(), digest.length());
  return shadow_root_.Append(obfuscated_username)
                     .Append(kKeyFile).AddExtension(safe_label);
}

FilePath Mount::GetUserEphemeralPath(
    const std::string& obfuscated_username) const {
  return FilePath(kEphemeralDir).Append(obfuscated_username);
}

FilePath Mount::GetUserVaultPath(
    const std::string& obfuscated_username) const {
  return shadow_root_.Append(obfuscated_username).Append(kVaultDir);
}

FilePath Mount::GetUserMountDirectory(
    const std::string& obfuscated_username) const {
  return shadow_root_.Append(obfuscated_username).Append(kMountDir);
}

FilePath Mount::GetUserTemporaryMountDirectory(
    const std::string& obfuscated_username) const {
  return shadow_root_.Append(obfuscated_username).Append(kTemporaryMountDir);
}

FilePath Mount::VaultPathToUserPath(const FilePath& vault) const {
  return vault.Append(kUserHomeSuffix);
}

FilePath Mount::VaultPathToRootPath(const FilePath& vault) const {
  return vault.Append(kRootHomeSuffix);
}

FilePath Mount::GetMountedUserHomePath(
    const std::string& obfuscated_username) const {
  return GetUserMountDirectory(obfuscated_username).Append(kUserHomeSuffix);
}

FilePath Mount::GetMountedRootHomePath(
    const std::string& obfuscated_username) const {
  return GetUserMountDirectory(obfuscated_username).Append(kRootHomeSuffix);
}

// TODO(dkrahn,wad) Makes this unique so we don't have to worry about
//                  parallelism.
FilePath Mount::GetEphemeralSkeletonPath() const {
  return shadow_root_.Append(kSkeletonDir);
}

std::string Mount::GetObfuscatedOwner() {
  EnsureDevicePolicyLoaded(false);
  std::string owner;
  if (policy_provider_->device_policy_is_loaded())
    policy_provider_->GetDevicePolicy().GetOwner(&owner);

  if (!owner.empty()) {
    return UsernamePasskey(owner.c_str(), brillo::Blob())
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

bool Mount::CheckChapsDirectory(const FilePath& dir,
                                const FilePath& legacy_dir) {
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
      LOG(INFO) << "Moving chaps directory from " << legacy_dir.value()
                << " to " << dir.value();
      if (!platform_->CopyWithPermissions(legacy_dir, dir)) {
        return false;
      }
      if (!platform_->DeleteFile(legacy_dir, true)) {
        PLOG(WARNING) << "Failed to clean up " << legacy_dir.value();
        return false;
      }
    } else {
      if (!platform_->CreateDirectory(dir)) {
        LOG(ERROR) << "Failed to create " << dir.value();
        return false;
      }
      if (!platform_->SetOwnership(dir,
                                   kChapsDirPermissions.user,
                                   kChapsDirPermissions.group,
                                   true)) {
        LOG(ERROR) << "Couldn't set file ownership for " << dir.value();
        return false;
      }
      if (!platform_->SetPermissions(dir, kChapsDirPermissions.mode)) {
        LOG(ERROR) << "Couldn't set permissions for " << dir.value();
        return false;
      }
    }
    return true;
  }
  // Directory already exists so check permissions and log a warning
  // if not as expected then attempt to apply correct permissions.
  std::map<FilePath, Platform::Permissions> special_cases;
  special_cases[dir.Append("auth_data_salt")] = kChapsSaltPermissions;
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
  if (!CheckChapsDirectory(token_dir, legacy_token_dir))
    return false;
  // We may create a salt file and, if so, we want to restrict access to it.
  ScopedUmask scoped_umask(platform_, kDefaultUmask);

  // Derive authorization data for the token from the passkey.
  FilePath salt_file = homedirs_->GetChapsTokenSaltPath(username);

  std::unique_ptr<chaps::TokenManagerClient> chaps_client(
      chaps_client_factory_->New());

  // If migration is required, send it before the login event.
  if (is_pkcs11_passkey_migration_required_) {
    LOG(INFO) << "Migrating authorization data.";
    SecureBlob old_auth_data;
    if (!crypto_->PasskeyToTokenAuthData(legacy_pkcs11_passkey_,
                                         salt_file,
                                         &old_auth_data))
      return false;
    chaps_client->ChangeTokenAuthData(
        token_dir,
        old_auth_data,
        pkcs11_token_auth_data_);
    is_pkcs11_passkey_migration_required_ = false;
    legacy_pkcs11_passkey_.clear();
  }

  Pkcs11Init pkcs11init;
  int slot_id = 0;
  if (!chaps_client->LoadToken(
      IsolateCredentialManager::GetDefaultIsolateCredential(),
      token_dir,
      pkcs11_token_auth_data_,
      pkcs11init.GetTpmTokenLabelForUser(current_user_->username()),
      &slot_id)) {
    LOG(ERROR) << "Failed to load PKCS #11 token.";
    ReportCryptohomeError(kLoadPkcs11TokenFailed);
  }
  pkcs11_token_auth_data_.clear();
  return true;
}

void Mount::RemovePkcs11Token() {
  std::string username = current_user_->username();
  FilePath token_dir = homedirs_->GetChapsTokenDir(username);
  std::unique_ptr<chaps::TokenManagerClient> chaps_client(
      chaps_client_factory_->New());
  chaps_client->UnloadToken(
      IsolateCredentialManager::GetDefaultIsolateCredential(),
      token_dir);
}

void Mount::MigrateToUserHome(const FilePath& vault_path) const {
  std::vector<FilePath> ent_list;
  std::vector<FilePath>::iterator ent_iter;
  FilePath user_path(VaultPathToUserPath(vault_path));
  FilePath root_path(VaultPathToRootPath(vault_path));
  struct stat st;

  // This check makes the migration idempotent; if we completed a migration,
  // root_path will exist and we're done, and if we didn't complete it, we can
  // finish it.
  if (platform_->Stat(root_path, &st) &&
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
  platform_->DeleteFile(root_path, true);

  // Get the list of entries before we create user_path, since user_path will be
  // inside dir.
  platform_->EnumerateDirectoryEntries(vault_path, false, &ent_list);

  if (!platform_->CreateDirectory(user_path)) {
    PLOG(ERROR) << "CreateDirectory() failed: " << user_path.value();
    return;
  }

  if (!platform_->SetOwnership(
          user_path, default_user_, default_group_, true)) {
    PLOG(ERROR) << "SetOwnership() failed: " << user_path.value();
    return;
  }

  for (const auto& ent : ent_list) {
    FilePath basename(ent);
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
    if (!platform_->Rename(next_path, dest_path)) {
      // TODO(ellyjones): UMA event log for this
      PLOG(WARNING) << "Migration fault: can't move " << next_path.value()
                    << " to " << dest_path.value();
    }
  }
  // Create root_path at the end as a sentinel for migration.
  if (!platform_->CreateDirectory(root_path)) {
    PLOG(ERROR) << "CreateDirectory() failed: " << root_path.value();
    return;
  }
  if (!platform_->SetOwnership(
          root_path, kMountOwnerUid, kDaemonStoreGid, true)) {
    PLOG(ERROR) << "SetOwnership() failed: " << root_path.value();
    return;
  }
  if (!platform_->SetPermissions(root_path, S_IRWXU | S_IRWXG | S_ISVTX)) {
    PLOG(ERROR) << "SetPermissions() failed: " << root_path.value();
    return;
  }
  LOG(INFO) << "Migrated (or created) user directory: " << vault_path.value();
}

void Mount::CopySkeleton(const FilePath& destination) const {
  RecursiveCopy(destination, FilePath(skel_source_));
}

bool Mount::CacheOldFiles(const std::vector<FilePath>& files) const {
  for (const auto& file : files) {
    FilePath file_bak = file.AddExtension("bak");
    if (platform_->FileExists(file_bak)) {
      if (!platform_->DeleteFile(file_bak, false)) {
        return false;
      }
    }
    if (platform_->FileExists(file)) {
      if (!platform_->Move(file, file_bak)) {
        return false;
      }
    }
  }
  return true;
}

void Mount::RecursiveCopy(const FilePath& destination,
                          const FilePath& source) const {
  std::unique_ptr<FileEnumerator> file_enumerator(
      platform_->GetFileEnumerator(source, false,
                                   base::FileEnumerator::FILES));
  FilePath next_path;
  while (!(next_path = file_enumerator->Next()).empty()) {
    FilePath file_name = next_path.BaseName();
    FilePath destination_file = destination.Append(file_name);
    if (!platform_->Copy(next_path, destination_file) ||
        !platform_->SetOwnership(
            destination_file, default_user_, default_group_, true)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of destination path: "
                 << destination_file.value();
    }
  }
  std::unique_ptr<FileEnumerator> dir_enumerator(
      platform_->GetFileEnumerator(source, false,
                                   base::FileEnumerator::DIRECTORIES));
  while (!(next_path = dir_enumerator->Next()).empty()) {
    FilePath dir_name = FilePath(next_path).BaseName();
    FilePath destination_dir = destination.Append(dir_name);
    LOG(INFO) << "RecursiveCopy: " << destination_dir.value();
    if (!platform_->CreateDirectory(destination_dir) ||
        !platform_->SetOwnership(
            destination_dir, default_user_, default_group_, true)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of destination path: "
                 << destination_dir.value();
    }
    RecursiveCopy(destination_dir, FilePath(next_path));
  }
}

bool Mount::RevertCacheFiles(const std::vector<FilePath>& files) const {
  for (const auto& file : files) {
    FilePath file_bak = file.AddExtension("bak");
    if (platform_->FileExists(file_bak)) {
      if (!platform_->Move(file_bak, file)) {
        return false;
      }
    }
  }
  return true;
}

bool Mount::DeleteCacheFiles(const std::vector<FilePath>& files) const {
  for (const auto& file : files) {
    FilePath file_bak = file.AddExtension("bak");
    if (platform_->FileExists(file_bak)) {
      if (!platform_->DeleteFile(file_bak, false)) {
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
  if (!platform_->Stat(check_path, &st)) {
    // Dirent not there, so create and set ownership.
    if (!platform_->CreateDirectory(check_path)) {
      PLOG(ERROR) << "Can't create: " << check_path.value();
      return false;
    }
    if (!platform_->SetOwnership(check_path, uid, gid, true)) {
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
  return platform_->CreateDirectory(fp);
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
  FilePath root_path = GetRootPath(username);
  FilePath user_path = GetUserPath(username);
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

std::unique_ptr<base::Value> Mount::GetStatus() {
  std::string user;
  SerializedVaultKeyset keyset;
  auto dv = base::MakeUnique<base::DictionaryValue>();
  current_user_->GetObfuscatedUsername(&user);
  auto keysets = base::MakeUnique<base::ListValue>();
  std::vector<int> key_indices;
  if (user.length() && homedirs_->GetVaultKeysets(user, &key_indices)) {
    for (auto key_index : key_indices) {
      auto keyset_dict = base::MakeUnique<base::DictionaryValue>();
      if (LoadVaultKeysetForUser(user, key_index, &keyset)) {
        bool tpm = keyset.flags() & SerializedVaultKeyset::TPM_WRAPPED;
        bool scrypt = keyset.flags() & SerializedVaultKeyset::SCRYPT_WRAPPED;
        keyset_dict->SetBoolean("tpm", tpm);
        keyset_dict->SetBoolean("scrypt", scrypt);
        keyset_dict->SetBoolean("ok", true);
        keyset_dict->SetInteger("last_activity",
                                keyset.last_activity_timestamp());
        if (keyset.has_key_data()) {
          // TODO(wad) Add additional KeyData
          keyset_dict->SetString("label", keyset.key_data().label());
        }
      } else {
        keyset_dict->SetBoolean("ok", false);
      }
      // TODO(wad) Replace key_index use with key_label() use once
      //           legacy keydata is populated.
      if (mount_type_ != MountType::EPHEMERAL &&
          key_index == current_user_->key_index())
        keyset_dict->SetBoolean("current", true);
      keyset_dict->SetInteger("index", key_index);
      keysets->Append(std::move(keyset_dict));
    }
  }
  dv->Set("keysets", std::move(keysets));
  dv->SetBoolean("mounted", IsMounted());
  dv->SetString("owner", GetObfuscatedOwner());
  dv->SetBoolean("enterprise", enterprise_owned_);

  std::string mount_type_string;
  switch (mount_type_) {
    case MountType::NONE:
      mount_type_string = "none";
      break;
    case MountType::ECRYPTFS:
      mount_type_string = "ecryptfs";
      break;
    case MountType::DIR_CRYPTO:
      mount_type_string = "dircrypto";
      break;
    case MountType::EPHEMERAL:
      mount_type_string = "ephemeral";
      break;
  }
  dv->SetString("type", mount_type_string);

  return std::move(dv);
}

// static
FilePath Mount::GetNewUserPath(const std::string& username) {
  std::string sanitized = SanitizeUserName(username);
  std::string user_dir = StringPrintf("u-%s", sanitized.c_str());
  return FilePath("/home").Append(kDefaultSharedUser).Append(user_dir);
}

bool Mount::MountLegacyHome(const FilePath& from, MountError* mount_error) {
  // Multiple mounts can't live on the legacy mountpoint.
  if (platform_->IsDirectoryMounted(FilePath(kDefaultHomeDir))) {
    LOG(INFO) << "Skipping binding to /home/chronos/user.";
  } else if (!RememberBind(from, FilePath(kDefaultHomeDir))) {
    PLOG(ERROR) << "Bind mount failed: " << from.value()
                << " -> " << kDefaultHomeDir;
    UnmountAll();
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  return true;
}

bool Mount::MigrateToDircrypto(
    const dircrypto_data_migrator::MigrationHelper::ProgressCallback&
    callback) {
  std::string obfuscated_username;
  current_user_->GetObfuscatedUsername(&obfuscated_username);
  FilePath temporary_mount =
      GetUserTemporaryMountDirectory(obfuscated_username);
  if (!IsMounted() || mount_type_ != MountType::DIR_CRYPTO ||
      !platform_->DirectoryExists(temporary_mount) ||
      !mounts_.ContainsDest(temporary_mount)) {
    LOG(ERROR) << "Not mounted for eCryptfs->dircrypto migration.";
    return false;
  }
  // Do migration.
  constexpr uint64_t kMaxChunkSize = 128 * 1024 * 1024;
  dircrypto_data_migrator::MigrationHelper migrator(
      platform_, GetUserDirectoryForUser(obfuscated_username), kMaxChunkSize);
  if (!migrator.Migrate(temporary_mount, mount_point_, callback)) {
    LOG(ERROR) << "Failed to migrate.";
    return false;
  }
  // Clean up.
  UnmountAll();
  FilePath vault_path = GetUserVaultPath(obfuscated_username);
  if (!platform_->DeleteFile(temporary_mount, true /* recursive */) ||
      !platform_->DeleteFile(vault_path, true /* recursive */)) {
    LOG(ERROR) << "Failed to delete the old vault.";
    return false;
  }
  return true;
}

}  // namespace cryptohome
