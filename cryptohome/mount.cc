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
#include <base/callback_helpers.h>
#include <base/files/file_path.h>
#include <base/logging.h>
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

#include "cryptohome/bootlockbox/boot_lockbox.h"
#include "cryptohome/chaps_client_factory.h"
#include "cryptohome/crypto.h"
#include "cryptohome/cryptohome_common.h"
#include "cryptohome/cryptohome_metrics.h"
#include "cryptohome/cryptolib.h"
#include "cryptohome/dircrypto_data_migrator/migration_helper.h"
#include "cryptohome/dircrypto_util.h"
#include "cryptohome/homedirs.h"
#include "cryptohome/obfuscated_username.h"
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

const char kDefaultHomeDir[] = "/home/chronos/user";
const char kDefaultShadowRoot[] = "/home/.shadow";
const char kEphemeralCryptohomeDir[] = "/run/cryptohome";
const char kSparseFileDir[] = "ephemeral_data";
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
const char kCacheDir[] = "Cache";
const char kDownloadsDir[] = "Downloads";
const char kMyFilesDir[] = "MyFiles";
const char kGCacheDir[] = "GCache";
const char kGCacheVersion1Dir[] = "v1";
const char kGCacheVersion2Dir[] = "v2";
const char kGCacheBlobsDir[] = "blobs";
const char kGCacheTmpDir[] = "tmp";
const char kUserHomeSuffix[] = "user";
const char kRootHomeSuffix[] = "root";
const char kEphemeralMountDir[] = "ephemeral_mount";
const char kTemporaryMountDir[] = "temporary_mount";
const char kKeyFile[] = "master";
const int kKeyFileMax = 100;  // master.0 ... master.99
const mode_t kKeyFilePermissions = 0600;
const char kKeyLegacyPrefix[] = "legacy-";
const char kEphemeralMountType[] = "ext4";
const char kEphemeralMountOptions[] = "";

const int kDefaultEcryptfsKeySize = CRYPTOHOME_AES_KEY_BYTES;
const gid_t kDaemonStoreGid = 400;

// User daemon store directories.
const char kRunDaemonStoreBaseDir[] = "/run/daemon-store/";
const char kEtcDaemonStoreBaseDir[] = "/etc/daemon-store/";

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
      shadow_only_(false),
      default_chaps_client_factory_(new ChapsClientFactory()),
      chaps_client_factory_(default_chaps_client_factory_.get()),
      boot_lockbox_(NULL),
      dircrypto_migration_stopped_condition_(&active_dircrypto_migrator_lock_) {
}

Mount::~Mount() {
  if (IsMounted())
    UnmountCryptohome();
}

bool Mount::Init(Platform* platform, Crypto* crypto,
                 UserOldestActivityTimestampCache *cache,
                 PreMountCallback pre_mount_callback) {
  platform_ = platform;
  crypto_ = crypto;
  user_timestamp_cache_ = cache;
  pre_mount_callback_ = pre_mount_callback;

  bool result = true;

  homedirs_->set_platform(platform_);
  homedirs_->set_shadow_root(FilePath(shadow_root_));
  homedirs_->set_enterprise_owned(enterprise_owned_);

  // Make sure |homedirs_| uses the same PolicyProvider instance as we in case
  // it was set by a test.
  if (policy_provider_)
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
  FilePath system_salt_file = shadow_root_.Append(kSystemSaltFile);
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
  if (!mount_args.shadow_only) {
    if (!EnsureUserMountPoints(credentials.username())) {
      return false;
    }
  }
  const std::string obfuscated_username =
      credentials.GetObfuscatedUsername(system_salt_);
  // Now check for the presence of a cryptohome.
  if (homedirs_->CryptohomeExists(obfuscated_username)) {
    // Now check for the presence of a vault directory.
    FilePath vault_path =
        homedirs_->GetEcryptfsUserVaultPath(obfuscated_username);
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
        LOG(ERROR) << "Unexpected state " << static_cast<int>(state);
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

bool Mount::MountCryptohome(const Credentials& credentials,
                            const Mount::MountArgs& mount_args,
                            MountError* mount_error) {
  CHECK(boot_lockbox_ || !use_tpm_);
  if (boot_lockbox_ && !boot_lockbox_->FinalizeBoot()) {
    LOG(WARNING) << "Failed to finalize boot lockbox.";
  }

  if (!pre_mount_callback_.is_null()) {
    pre_mount_callback_.Run();
  }

  if (IsMounted()) {
    if (mount_error)
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
  *key_signature = CryptoLib::SecureBlobToHex(vault_keyset.fek_sig());
  if (!platform_->AddEcryptfsAuthToken(
        vault_keyset.fek(), *key_signature,
        vault_keyset.fek_salt())) {
    LOG(ERROR) << "Couldn't add ecryptfs key to keyring";
    return false;
  }

  // Add the File Name Encryption Key (FNEK) from the vault keyset.  This is the
  // key that is used to encrypt the file name when it is persisted to the lower
  // filesystem by eCryptfs.
  *filename_key_signature = CryptoLib::SecureBlobToHex(vault_keyset.fnek_sig());
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
                                 bool recreate_on_decrypt_fatal,
                                 MountError* mount_error) {
  current_user_->Reset();

  std::string username = credentials.username();
  if (username.compare(kIncognitoUser) == 0) {
    // TODO(fes): Have guest set error conditions?
    *mount_error = MOUNT_ERROR_NONE;
    return MountGuestCryptohome();
  }

  // Remove all existing cryptohomes, except for the owner's one, if the
  // ephemeral users policy is on.
  // Note that a fresh policy value is read here, which in theory can conflict
  // with the one used for calculation of |mount_args.is_ephemeral|. However,
  // this inconsistency (whose probability is anyway pretty low in practice)
  // should only lead to insignificant transient glitches, like an attempt to
  // mount a non existing anymore cryptohome.
  if (homedirs_->AreEphemeralUsersEnabled())
    homedirs_->RemoveNonOwnerCryptohomes();

  const std::string obfuscated_username =
      credentials.GetObfuscatedUsername(system_salt_);
  const bool is_owner = homedirs_->IsOrWillBeOwner(username);

  // Process ephemeral mounts in a special manner.
  if (mount_args.is_ephemeral) {
    if (!mount_args.create_if_missing) {
      NOTREACHED() << "An ephemeral cryptohome can only be mounted when its "
                      "creation on-the-fly is allowed.";
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }

    if (is_owner) {
      LOG(ERROR) << "An ephemeral cryptohome can only be mounted when the user "
                    "is not the owner.";
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }

    if (!MountEphemeralCryptohome(credentials.username())) {
      homedirs_->Remove(credentials.username());
      *mount_error = MOUNT_ERROR_FATAL;
      return false;
    }

    // Ephemeral and guest users will not have a key index.
    current_user_->SetUser(credentials);
    *mount_error = MOUNT_ERROR_NONE;
    return true;
  }

  if (!mount_args.create_if_missing &&
      !homedirs_->CryptohomeExists(obfuscated_username)) {
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
  if (!DecryptVaultKeyset(credentials, &vault_keyset, &serialized, &index,
                          &local_mount_error)) {
    *mount_error = local_mount_error;
    if (recreate_on_decrypt_fatal && (local_mount_error & MOUNT_ERROR_FATAL)) {
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

  // It's safe to generate a reset_seed here.
  if (!serialized.has_wrapped_reset_seed()) {
    vault_keyset.CreateRandomResetSeed();
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
  if (homedirs_->EcryptfsCryptohomeExists(obfuscated_username) &&
      homedirs_->DircryptoCryptohomeExists(obfuscated_username) &&
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
    NOTREACHED() << "Unexpected mount type " << static_cast<int>(mount_type_);
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
      UnmountAll();
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

  FilePath vault_path =
      homedirs_->GetEcryptfsUserVaultPath(obfuscated_username);

  mount_point_ = homedirs_->GetUserMountDirectory(obfuscated_username);
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
      LOG(ERROR) << "Cryptohome mount failed";
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

  const FilePath user_home = GetMountedUserHomePath(obfuscated_username);
  const FilePath root_home = GetMountedRootHomePath(obfuscated_username);
  if (created)
    CopySkeleton(user_home);

  if (!SetupGroupAccess(FilePath(user_home))) {
    UnmountAll();
    *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }

  shadow_only_ = mount_args.shadow_only;

  // When migrating, it's better to avoid exposing the new ext4 crypto dir.
  // Also don't expose home directory if a shadow-only mount was requested.
  if (!mount_args.to_migrate_from_ecryptfs && !mount_args.shadow_only &&
      !MountHomesAndDaemonStores(username, obfuscated_username, user_home,
                                 root_home)) {
    UnmountAll();
    *mount_error = MOUNT_ERROR_FATAL;
    return false;
  }

  if (!UserSignInEffects(true /* is_mount */, is_owner)) {
    LOG(ERROR) << "Failed to set user type, aborting mount";
    UnmountAll();
    *mount_error = MOUNT_ERROR_TPM_COMM_ERROR;
    return false;
  }

  // TODO(ellyjones): Expose the path to the root directory over dbus for use by
  // daemons. We may also want to bind-mount it somewhere stable.

  *mount_error = MOUNT_ERROR_NONE;

  switch (mount_type_) {
    case MountType::ECRYPTFS:
      ReportHomedirEncryptionType(HomedirEncryptionType::kEcryptfs);
      break;
    case MountType::DIR_CRYPTO:
      ReportHomedirEncryptionType(HomedirEncryptionType::kDircrypto);
      break;
    default:
      // We're only interested in encrypted home directories.
      NOTREACHED() << "Unknown homedir encryption type: "
                   << static_cast<int>(mount_type_);
      break;
  }

  if (is_pkcs11_passkey_migration_required_) {
    credentials.GetPasskey(&legacy_pkcs11_passkey_);
  }

  // TODO(fqj,b/116072767) Ignore errors since unlabeled files are currently
  // still okay during current development progress.
  LOG(INFO) << "Restoring selinux context for homedir.";
  platform_->RestoreSELinuxContexts(
      homedirs_->GetUserMountDirectory(obfuscated_username),
      true);

  return true;
}

void Mount::CleanUpEphemeral() {
  if (!ephemeral_loop_device_.empty()) {
    if (!platform_->DetachLoop(ephemeral_loop_device_)) {
      ReportCryptohomeError(kEphemeralCleanUpFailed);
      PLOG(ERROR) << "Can't detach loop: " << ephemeral_loop_device_.value();
    }
    ephemeral_loop_device_.clear();
  }
  if (!ephemeral_file_path_.empty()) {
    if (!platform_->DeleteFile(ephemeral_file_path_, false /* recursive */)) {
      ReportCryptohomeError(kEphemeralCleanUpFailed);
      PLOG(ERROR) << "Failed to clean up ephemeral sparse file: "
                  << ephemeral_file_path_.value();
    }
    ephemeral_file_path_.clear();
  }
}

bool Mount::MountEphemeralCryptohome(const std::string& username) {
  // Ephemeral cryptohome can't be mounted twice.
  CHECK(ephemeral_file_path_.empty());
  CHECK(ephemeral_loop_device_.empty());

  bool mounted = MountEphemeralCryptohomeInner(username);
  if (!mounted) {
    UnmountAll();
    CleanUpEphemeral();
  }
  return mounted;
}

bool Mount::MountEphemeralCryptohomeInner(const std::string& username) {
  // Underlying sparse file will be created in a temporary directory in RAM.
  const FilePath ephemeral_root(kEphemeralCryptohomeDir);

  // Determine ephemeral cryptohome size.
  struct statvfs vfs;
  if (!platform_->StatVFS(ephemeral_root, &vfs)) {
    PLOG(ERROR) << "Can't determine ephemeral cryptohome size";
    return false;
  }
  const size_t sparse_size = vfs.f_blocks * vfs.f_frsize;

  // Create underlying sparse file
  const std::string obfuscated_username =
      BuildObfuscatedUsername(username, system_salt_);
  const FilePath sparse_file = GetEphemeralSparseFile(obfuscated_username);
  if (!platform_->CreateDirectory(sparse_file.DirName())) {
    LOG(ERROR) << "Can't create directory for ephemeral sparse files";
    return false;
  }

  // Remember the file to clean up if error will happen during file creation.
  ephemeral_file_path_ = sparse_file;
  if (!platform_->CreateSparseFile(sparse_file, sparse_size)) {
    LOG(ERROR) << "Can't create ephemeral sparse file";
    return false;
  }

  // Format the sparse file into ext4.
  if (!platform_->FormatExt4(sparse_file)) {
    LOG(ERROR) << "Can't format ephemeral sparse file into ext4";
    return false;
  }

  // Create a loop device based on the sparse file.
  ephemeral_loop_device_ = platform_->AttachLoop(sparse_file);
  if (ephemeral_loop_device_.empty()) {
    LOG(ERROR) << "Can't create loop device";
    return false;
  }

  const FilePath mount_point =
      GetUserEphemeralMountDirectory(obfuscated_username);
  if (!platform_->CreateDirectory(mount_point)) {
    PLOG(ERROR) << "Directory creation failed for " << mount_point.value();
    return false;
  }
  if (!RememberMount(ephemeral_loop_device_,
                     mount_point,
                     kEphemeralMountType,
                     kEphemeralMountOptions)) {
    LOG(ERROR) << "Can't mount ephemeral mount point";
    return false;
  }

  // Create user & root directories.
  MigrateToUserHome(mount_point);
  if (!EnsureUserMountPoints(username)) {
    return false;
  }

  const FilePath user_home =
      GetMountedEphemeralUserHomePath(obfuscated_username);
  const FilePath root_home =
      GetMountedEphemeralRootHomePath(obfuscated_username);

  if (!SetUpEphemeralCryptohome(user_home))
    return false;

  if (!MountHomesAndDaemonStores(username, obfuscated_username, user_home,
                                 root_home)) {
    return false;
  }

  if (!UserSignInEffects(true /* is_mount */, false /* is_owner */)) {
    LOG(ERROR) << "Failed to set user type, aborting ephemeral mount";
    return false;
  }

  mount_type_ = MountType::EPHEMERAL;
  return true;
}

bool Mount::SetUpEphemeralCryptohome(const FilePath& source_path) {
  CopySkeleton(source_path);

  // Create the Downloads, MyFiles, MyFiles/Downloads, GCache and GCache/v2
  // directories if they don't exist so they can be made group accessible when
  // SetupGroupAccess() is called.
  const FilePath user_files_paths[] = {
      FilePath(source_path).Append(kDownloadsDir),
      FilePath(source_path).Append(kMyFilesDir),
      FilePath(source_path).Append(kMyFilesDir).Append(kDownloadsDir),
      FilePath(source_path).Append(kGCacheDir),
      FilePath(source_path).Append(kGCacheDir).Append(kGCacheVersion2Dir),
  };
  for (const auto& path : user_files_paths) {
    if (platform_->DirectoryExists(path))
        continue;

    if (!platform_->CreateDirectory(path) ||
        !platform_->SetOwnership(path, default_user_, default_group_, true)) {
      LOG(ERROR) << "Couldn't create user path directory: " << path.value();
      return false;
    }
  }

  if (!platform_->SetOwnership(source_path,
                               default_user_,
                               default_access_group_,
                               true)) {
    LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
               << default_access_group_ << ") of path: "
               << source_path.value();
    return false;
  }

  if (!SetupGroupAccess(FilePath(source_path)))
    return false;

  return true;
}

bool Mount::RememberMount(const FilePath& src,
                          const FilePath& dest, const std::string& type,
                          const std::string& options) {
  if (!platform_->Mount(src, dest, type, options)) {
    PLOG(ERROR) << "Mount failed: " << src.value() << " -> " << dest.value();
    return false;
  }

  mounts_.Push(src, dest);
  return true;
}

bool Mount::RememberBind(const FilePath& src,
                         const FilePath& dest) {
  if (!platform_->Bind(src, dest)) {
    PLOG(ERROR) << "Bind mount failed: " << src.value() << " -> "
                << dest.value();
    return false;
  }

  mounts_.Push(src, dest);
  return true;
}

void Mount::UnmountAll() {
  FilePath src, dest;
  const FilePath ephemeral_mount_path =
      FilePath(kEphemeralCryptohomeDir).Append(kEphemeralMountDir);
  while (mounts_.Pop(&src, &dest)) {
    ForceUnmount(src, dest);
    // Clean up destination directory for ephemeral loop device mounts.
    if (ephemeral_mount_path.IsParent(dest))
      platform_->DeleteFile(dest, true /* recursive */);
  }

  // Invalidate dircrypto key to make directory contents inaccessible.
  if (dircrypto_key_id_ != dircrypto::kInvalidKeySerial) {
    platform_->InvalidateDirCryptoKey(dircrypto_key_id_, shadow_root_);
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
  if (!UserSignInEffects(false /* is_mount */, false /* is_owner */)) {
    LOG(WARNING) << "Failed to set user type, but continuing with unmount";
  }

  // There should be no file access when unmounting.
  // Stop dircrypto migration if in progress.
  MaybeCancelActiveDircryptoMigrationAndWait();

  UnmountAll();
  CleanUpEphemeral();
  if (homedirs_->AreEphemeralUsersEnabled())
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
    FilePath vault_path = homedirs_->GetEcryptfsUserVaultPath(
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
      FilePath(kUserHomeSuffix).Append(kMyFilesDir),
      FilePath(kUserHomeSuffix).Append(kMyFilesDir).Append(kDownloadsDir),
      FilePath(kUserHomeSuffix).Append(kGCacheDir),
      FilePath(kUserHomeSuffix).Append(kGCacheDir).Append(kGCacheVersion1Dir),
      FilePath(kUserHomeSuffix).Append(kGCacheDir).Append(kGCacheVersion2Dir),
      FilePath(kUserHomeSuffix)
          .Append(kGCacheDir)
          .Append(kGCacheVersion1Dir)
          .Append(kGCacheBlobsDir),
      FilePath(kUserHomeSuffix)
          .Append(kGCacheDir)
          .Append(kGCacheVersion1Dir)
          .Append(kGCacheTmpDir),
  };
}

bool Mount::CreateTrackedSubdirectories(const Credentials& credentials,
                                        bool is_new) const {
  ScopedUmask scoped_umask(platform_, kDefaultUmask);

  // Add the subdirectories if they do not exist.
  const std::string obfuscated_username =
      credentials.GetObfuscatedUsername(system_salt_);
  const FilePath dest_dir(
      mount_type_ == MountType::ECRYPTFS
          ? homedirs_->GetEcryptfsUserVaultPath(obfuscated_username)
          : homedirs_->GetUserMountDirectory(obfuscated_username));
  if (!platform_->DirectoryExists(dest_dir)) {
     LOG(ERROR) << "Can't create tracked subdirectories for a missing user.";
     return false;
  }

  const FilePath mount_dir(
      homedirs_->GetUserMountDirectory(obfuscated_username));

  // The call is allowed to partially fail if directory creation fails, but we
  // want to have as many of the specified tracked directories created as
  // possible.
  bool result = true;
  for (const auto& tracked_dir : GetTrackedSubdirectories()) {
    const FilePath tracked_dir_path = dest_dir.Append(tracked_dir);
    if (mount_type_ == MountType::ECRYPTFS) {
      const FilePath userside_dir = mount_dir.Append(tracked_dir);
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

bool Mount::BindMyFilesDownloads(const base::FilePath& user_home) {
  if (!platform_->DirectoryExists(user_home)) {
    LOG(ERROR) << "Failed to bind MyFiles/Downloads missing directory: "
               << user_home.value();
    return false;
  }

  const FilePath downloads = user_home.Append(kDownloadsDir);
  if (!platform_->DirectoryExists(downloads)) {
    LOG(INFO) << "Failed to bind MyFiles/Downloads missing directory: "
              << downloads.value();
    return false;
  }

  const FilePath downloads_in_myfiles =
      user_home.Append(kMyFilesDir).Append(kDownloadsDir);
  if (!platform_->DirectoryExists(downloads_in_myfiles)) {
    LOG(INFO) << "Failed to bind MyFiles/Downloads, missing directory: "
              << downloads_in_myfiles.value();
    return false;
  }

  if (!RememberBind(downloads, downloads_in_myfiles))
    return false;

  return true;
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

bool Mount::SetupGroupAccess(const FilePath& home_dir) const {
  // Make the following directories group accessible by other system daemons:
  //   {home_dir}
  //   {home_dir}/Downloads
  //   {home_dir}/MyFiles
  //   {home_dir}/MyFiles/Downloads
  //   {home_dir}/GCache
  //   {home_dir}/GCache/v1 (only if it exists)
  //
  // Make the following directories group accessible and writable by other
  // system daemons:
  //   {home_dir}/GCache/v2
  const struct {
    FilePath path;
    bool optional = false;
    bool group_writable = false;
  } kGroupAccessiblePaths[] = {
      {home_dir},
      {home_dir.Append(kDownloadsDir)},
      {home_dir.Append(kMyFilesDir)},
      {home_dir.Append(kMyFilesDir).Append(kDownloadsDir)},
      {home_dir.Append(kGCacheDir)},
      {home_dir.Append(kGCacheDir).Append(kGCacheVersion1Dir), true},
      {home_dir.Append(kGCacheDir).Append(kGCacheVersion2Dir), false, true},
  };

  constexpr mode_t kDefaultMode = S_IXGRP;
  constexpr mode_t kWritableMode = kDefaultMode | S_IWGRP;
  for (const auto& accessible : kGroupAccessiblePaths) {
    if (!platform_->FileExists(accessible.path) &&
        accessible.optional)
      continue;

    if (!platform_->SetGroupAccessible(
            accessible.path, default_access_group_,
            accessible.group_writable ? kWritableMode : kDefaultMode)) {
      return false;
    }
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
  // Load the encrypted keyset.
  FilePath user_key_file =
      GetUserLegacyKeyFileForUser(obfuscated_username, index);
  if (!platform_->FileExists(user_key_file)) {
    return false;
  }
  brillo::Blob cipher_text;
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
  return platform_->WriteSecureBlobToFileAtomicDurable(
      GetUserLegacyKeyFileForUser(obfuscated_username, index),
      final_blob,
      kKeyFilePermissions);
}

bool Mount::DecryptVaultKeyset(const Credentials& credentials,
                               VaultKeyset* vault_keyset,
                               SerializedVaultKeyset* serialized,
                               int* index,
                               MountError* error) const {
  *error = MOUNT_ERROR_NONE;

  if (!homedirs_->GetValidKeyset(credentials, vault_keyset, index, error))
    return false;
  *serialized = vault_keyset->serialized();

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
  // In the table below: X = true, - = false, * = any value
  //
  //                 1   2   3   4   5   6   7   8
  // should_tpm      X   X   X   X   -   -   -   *
  //
  // tpm_wrapped     -   X   X   -   -   X   -   X
  //
  // scrypt_wrapped  -   -   -   X   -   -   X   X
  //
  // scrypt_derived  *   X   -   *   *   *   *   *
  //
  // migrate         Y   N   Y   Y   Y   Y   N   Y
  //
  // If the vault keyset represents an LE credential, we should not re-encrypt
  // it at all (that is unnecessary).
  const unsigned crypt_flags = serialized->flags();
  bool tpm_wrapped =
      (crypt_flags & SerializedVaultKeyset::TPM_WRAPPED) != 0;
  bool scrypt_wrapped =
      (crypt_flags & SerializedVaultKeyset::SCRYPT_WRAPPED) != 0;
  bool scrypt_derived =
      (crypt_flags & SerializedVaultKeyset::SCRYPT_DERIVED) != 0;
  bool should_tpm = (crypto_->has_tpm() && use_tpm_ &&
                     crypto_->is_cryptohome_key_loaded());
  bool is_le_credential =
      (crypt_flags & SerializedVaultKeyset::LE_CREDENTIAL) != 0;
  do {
    // If the keyset was TPM-wrapped, but there was no public key hash,
    // always re-save.  Otherwise, check the table.
    if (serialized->has_tpm_public_key_hash() || is_le_credential) {
      if (is_le_credential && !crypto_->NeedsPcrBinding(serialized->le_label()))
        break;
      if (tpm_wrapped && should_tpm && !scrypt_wrapped) {
        if (scrypt_derived)
          break;  // 2
        LOG(INFO) << "Migrating to deriving AES keys using scrypt.";
      }
      if (scrypt_wrapped && !should_tpm && !tpm_wrapped)
        break;  // 7
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

  std::string obfuscated_username =
      credentials.GetObfuscatedUsername(system_salt_);

  // Encrypt the vault keyset
  SecureBlob salt(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  CryptoLib::GetSecureRandom(salt.data(), salt.size());

  if (!crypto_->EncryptVaultKeyset(vault_keyset, passkey, salt,
                                   obfuscated_username, serialized)) {
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
  uint64_t label = serialized->le_label();
  if (!AddVaultKeyset(credentials, vault_keyset, serialized)) {
    LOG(ERROR) << "Couldn't add keyset.";
    RevertCacheFiles(files);
    return false;
  }

  if ((serialized->flags() & SerializedVaultKeyset::LE_CREDENTIAL) != 0) {
    if (!crypto_->RemoveLECredential(label)) {
      // This is non-fatal error.
      LOG(ERROR) << "Failed to remove label = " << label;
    }
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

  if (!pre_mount_callback_.is_null()) {
    pre_mount_callback_.Run();
  }

  current_user_->Reset();
  return MountEphemeralCryptohome(kGuestUserName);
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

FilePath Mount::GetUserEphemeralMountDirectory(
    const std::string& obfuscated_username) const {
  return FilePath(kEphemeralCryptohomeDir).Append(kEphemeralMountDir)
      .Append(obfuscated_username);
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
  return homedirs_->GetUserMountDirectory(obfuscated_username)
      .Append(kUserHomeSuffix);
}

FilePath Mount::GetMountedRootHomePath(
    const std::string& obfuscated_username) const {
  return homedirs_->GetUserMountDirectory(obfuscated_username)
      .Append(kRootHomeSuffix);
}

FilePath Mount::GetMountedEphemeralUserHomePath(
    const std::string& obfuscated_username) const {
  return GetUserEphemeralMountDirectory(obfuscated_username)
      .Append(kUserHomeSuffix);
}

FilePath Mount::GetMountedEphemeralRootHomePath(
    const std::string& obfuscated_username) const {
  return GetUserEphemeralMountDirectory(obfuscated_username)
      .Append(kRootHomeSuffix);
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
  ReportTimerStop(kPkcs11InitTimer);
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

bool Mount::EnsureUserMountPoints(const std::string& username) const {
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
  auto dv = std::make_unique<base::DictionaryValue>();
  current_user_->GetObfuscatedUsername(&user);
  auto keysets = std::make_unique<base::ListValue>();
  std::vector<int> key_indices;
  if (user.length() && homedirs_->GetVaultKeysets(user, &key_indices)) {
    for (auto key_index : key_indices) {
      auto keyset_dict = std::make_unique<base::DictionaryValue>();
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
  std::string obfuscated_owner;
  homedirs_->GetOwner(&obfuscated_owner);
  dv->SetString("owner", obfuscated_owner);
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

FilePath Mount::GetEphemeralSparseFile(const std::string& obfuscated_username) {
  return FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir)
      .Append(obfuscated_username);
}

bool Mount::MountLegacyHome(const FilePath& from) {
  // Multiple mounts can't live on the legacy mountpoint.
  if (platform_->IsDirectoryMounted(FilePath(kDefaultHomeDir))) {
    LOG(INFO) << "Skipping binding to /home/chronos/user.";
    return true;
  }

  if (!RememberBind(from, FilePath(kDefaultHomeDir)))
    return false;

  return true;
}

bool Mount::MigrateToDircrypto(
    const dircrypto_data_migrator::MigrationHelper::ProgressCallback& callback,
    MigrationType migration_type) {
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
      platform_,
      temporary_mount,
      mount_point_,
      GetUserDirectoryForUser(obfuscated_username),
      kMaxChunkSize,
      migration_type);
  {  // Abort if already cancelled.
    base::AutoLock lock(active_dircrypto_migrator_lock_);
    if (is_dircrypto_migration_cancelled_)
      return false;
    CHECK(!active_dircrypto_migrator_);
    active_dircrypto_migrator_ = &migrator;
  }
  bool success = migrator.Migrate(callback);
  UnmountAll();
  {  // Signal the waiting thread.
    base::AutoLock lock(active_dircrypto_migrator_lock_);
    active_dircrypto_migrator_ = nullptr;
    dircrypto_migration_stopped_condition_.Signal();
  }
  if (!success) {
    LOG(ERROR) << "Failed to migrate.";
    return false;
  }
  // Clean up.
  FilePath vault_path =
      homedirs_->GetEcryptfsUserVaultPath(obfuscated_username);
  if (!platform_->DeleteFile(temporary_mount, true /* recursive */) ||
      !platform_->DeleteFile(vault_path, true /* recursive */)) {
    LOG(ERROR) << "Failed to delete the old vault.";
    return false;
  }
  return true;
}

void Mount::MaybeCancelActiveDircryptoMigrationAndWait() {
  base::AutoLock lock(active_dircrypto_migrator_lock_);
  is_dircrypto_migration_cancelled_ = true;
  while (active_dircrypto_migrator_) {
    active_dircrypto_migrator_->Cancel();
    LOG(INFO) << "Waiting for dircrypto migration to stop.";
    dircrypto_migration_stopped_condition_.Wait();
    LOG(INFO) << "Dircrypto migration stopped.";
  }
}

bool Mount::IsShadowOnly() const { return shadow_only_; }

// TODO(chromium:795310): include all side-effects and move out of mount.cc.
// Sign-in/sign-out effects hook.
// Performs actions that need to follow a mount/unmount operation as a part of
// user sign-in/sign-out.
// Parameters:
//   |mount| - the mount instance that was just mounted/unmounted.
//   |tpm| - the TPM instance.
//   |is_mount| - true for mount operation, false for unmount.
//   |is_owner| - true if mounted for an owner user, false otherwise.
// Returns true if successful, false otherwise.
bool Mount::UserSignInEffects(bool is_mount, bool is_owner) {
  Tpm* tpm = crypto_->get_tpm();
  if (!tpm) {
    return true;
  }

  Tpm::UserType user_type =
      (is_mount & is_owner) ? Tpm::UserType::Owner : Tpm::UserType::NonOwner;
  return tpm->SetUserType(user_type);
}

bool Mount::MountHomesAndDaemonStores(const std::string& username,
                                      const std::string& obfuscated_username,
                                      const FilePath& user_home,
                                      const FilePath& root_home) {
  // Mount /home/chronos/user.
  if (legacy_mount_ && !MountLegacyHome(user_home))
    return false;

  // Mount  /home/chronos/u-<user_hash>
  const FilePath new_user_path = GetNewUserPath(username);
  if (!RememberBind(user_home, new_user_path))
    return false;

  // Mount /home/user/<user_hash>.
  const FilePath user_multi_home = GetUserPath(username);
  if (!RememberBind(user_home, user_multi_home))
    return false;

  // Mount /home/root/<user_hash>.
  const FilePath root_multi_home = GetRootPath(username);
  if (!RememberBind(root_home, root_multi_home))
    return false;

  // Mount Downloads to MyFiles/Downloads in:
  //  - /home/chronos/u-<user_hash>
  //  - /home/user/<user_hash>
  //  - /home/chronos/user
  if (!(BindMyFilesDownloads(new_user_path) &&
        BindMyFilesDownloads(user_multi_home))) {
    return false;
  }

  if (legacy_mount_ && !BindMyFilesDownloads(FilePath(kDefaultHomeDir)))
    return false;

  // Mount directories used by daemons to store per-user data.
  if (!MountDaemonStoreDirectories(root_home, obfuscated_username))
    return false;

  return true;
}

bool Mount::MountDaemonStoreDirectories(
    const FilePath& root_home, const std::string& obfuscated_username) {
  // Iterate over all directories in /etc/daemon-store. This list is on rootfs,
  // so it's tamper-proof and nobody can sneak in additional directories that we
  // blindly mount. The actual mounts happen on /run/daemon-store, though.
  std::unique_ptr<FileEnumerator> file_enumerator(platform_->GetFileEnumerator(
      FilePath(kEtcDaemonStoreBaseDir), false /* recursive */,
      base::FileEnumerator::DIRECTORIES));

  // /etc/daemon-store/<daemon-name>
  FilePath etc_daemon_store_path;
  while (!(etc_daemon_store_path = file_enumerator->Next()).empty()) {
    const FilePath& daemon_name = etc_daemon_store_path.BaseName();

    // /run/daemon-store/<daemon-name>
    FilePath run_daemon_store_path =
        FilePath(kRunDaemonStoreBaseDir).Append(daemon_name);
    if (!platform_->DirectoryExists(run_daemon_store_path)) {
      // The chromeos_startup script should make sure this exist.
      PLOG(ERROR) << "Daemon store directory does not exist: "
                  << run_daemon_store_path.value();
      return false;
    }

    // /home/.shadow/<user_hash>/mount/root/<daemon-name>
    const FilePath mount_source = root_home.Append(daemon_name);

    // /run/daemon-store/<daemon-name>/<user_hash>
    const FilePath mount_target =
        run_daemon_store_path.Append(obfuscated_username);

    if (!platform_->CreateDirectory(mount_source)) {
      PLOG(ERROR) << "Directory creation failed for " << mount_source.value();
      return false;
    }

    if (!platform_->CreateDirectory(mount_target)) {
      PLOG(ERROR) << "Directory creation failed for " << mount_target.value();
      return false;
    }

    // Copy ownership from |etc_daemon_store_path| to |mount_source|. After the
    // bind operation, this guarantees that ownership for |mount_target| is the
    // same as for |etc_daemon_store_path| (usually
    // <daemon_user>:<daemon_group>), which is what the daemon intended.
    // Otherwise, it would end up being root-owned.
    struct stat etc_daemon_path_stat = file_enumerator->GetInfo().stat();
    if (!platform_->SetOwnership(mount_source, etc_daemon_path_stat.st_uid,
                                 etc_daemon_path_stat.st_gid,
                                 false /* follow_links */)) {
      PLOG(ERROR) << "Failed to set ownership for " << mount_source.value();
      return false;
    }

    // Similarly, transfer directory permissions. Should usually be 0700, so
    // that only the daemon has full access.
    if (!platform_->SetPermissions(mount_source,
                                   etc_daemon_path_stat.st_mode)) {
      PLOG(ERROR) << "Failed to set permissions for " << mount_source.value();
      return false;
    }

    // Assuming that |run_daemon_store_path| is a shared mount and the daemon
    // runs in a file system namespace with |run_daemon_store_path| mounted as
    // slave, this mount event propagates into the daemon.
    if (!RememberBind(mount_source, mount_target))
      return false;
  }

  return true;
}

}  // namespace cryptohome
