// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

#include "mount.h"

#include <errno.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/platform_thread.h>
#include <base/time.h>
#include <base/string_util.h>
#include <base/values.h>
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

const std::string kDefaultHomeDir = "/home/chronos/user";
const std::string kDefaultShadowRoot = "/home/.shadow";
const std::string kDefaultSharedUser = "chronos";
const std::string kDefaultSkeletonSource = "/etc/skel";
// TODO(fes): Remove once UI for BWSI switches to MountGuest()
const std::string kIncognitoUser = "incognito";
// The length of a user's directory name in the shadow root (equal to the length
// of the ascii version of a SHA1 hash)
const unsigned int kUserDirNameLength = 40;
// Encrypted files/directories in ecryptfs have file names that start with the
// following constant.  When clearing tracked subdirectories, we ignore these
// and only delete the pass-through directories.
const std::string kEncryptedFilePrefix = "ECRYPTFS_FNEK_ENCRYPTED.";

Mount::Mount()
    : default_user_(-1),
      default_group_(-1),
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
      current_user_(default_current_user_.get()) {
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
                            MountError* mount_error) const {
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
                                 bool recreate_unwrap_fatal,
                                 MountError* mount_error) const {
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

  // Attempt to unwrap the vault keyset with the specified credentials
  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  MountError local_mount_error = MOUNT_ERROR_NONE;
  if (!UnwrapVaultKeyset(credentials, true, &vault_keyset, &serialized,
                         &local_mount_error)) {
    if (mount_error) {
      *mount_error = local_mount_error;
    }
    if (recreate_unwrap_fatal && (local_mount_error & MOUNT_ERROR_FATAL)) {
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

  if (mount_args.replace_tracked_subdirectories) {
    if (ReplaceTrackedSubdirectories(mount_args.tracked_subdirectories,
                                     &serialized)) {
      // If the tracked subdirectories changed, re-save the vault keyset
      StoreVaultKeyset(credentials, serialized);
    }
  }

  // Ensure that the tracked subdirectories exist
  CreateTrackedSubdirectories(credentials, serialized);

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
                                         CRYPTOHOME_AES_KEY_BYTES,
                                         fnek_signature.c_str(),
                                         key_signature.c_str());

  // Mount cryptohome
  string vault_path = GetUserVaultPath(credentials);
  if (!platform_->Mount(vault_path, home_dir_, "ecryptfs", ecryptfs_options)) {
    LOG(INFO) << "Cryptohome mount failed: " << errno << ", for vault: "
               << vault_path;
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  if (created) {
    CopySkeletonForUser(credentials);
  }

  if (mount_error) {
    *mount_error = MOUNT_ERROR_NONE;
  }

  current_user_->SetUser(credentials);
  return true;
}

bool Mount::UnmountCryptohome() const {
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

  // Clear the user keyring if the unmount was successful
  crypto_->ClearKeyset();

  return true;
}

bool Mount::RemoveCryptohome(const Credentials& credentials) const {
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

  // Generat a new master key
  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(*this);
  SerializedVaultKeyset serialized;
  ReplaceTrackedSubdirectories(mount_args.tracked_subdirectories, &serialized);
  if (!AddVaultKeyset(credentials, vault_keyset, &serialized)) {
    platform_->SetMask(original_mask);
    return false;
  }
  if (!StoreVaultKeyset(credentials, serialized)) {
    platform_->SetMask(original_mask);
    return false;
  }

  // Create the user's path and set the proper ownership
  std::string vault_path = GetUserVaultPath(credentials);
  if (!file_util::CreateDirectory(FilePath(vault_path))) {
    LOG(ERROR) << "Couldn't create vault path: " << vault_path.c_str();
    platform_->SetMask(original_mask);
    return false;
  }
  if (set_vault_ownership_) {
    if (!platform_->SetOwnership(vault_path, default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of vault path: "
                 << vault_path.c_str();
      platform_->SetMask(original_mask);
      return false;
    }
  }

  CreateTrackedSubdirectories(credentials, serialized);

  // Restore the umask
  platform_->SetMask(original_mask);
  return true;
}

bool Mount::CreateTrackedSubdirectories(const Credentials& credentials,
      const SerializedVaultKeyset& serialized) const {
  if (serialized.tracked_subdirectories_size() == 0) {
    return true;
  }

  int original_mask = platform_->SetMask(kDefaultUmask);

  // Add the subdirectories if they do not exist
  FilePath vault_path = FilePath(GetUserVaultPath(credentials));
  if (!file_util::DirectoryExists(vault_path)) {
    LOG(ERROR) << "Can't create tracked subdirectories for a missing user.";
    platform_->SetMask(original_mask);
    return false;
  }

  // The call is allowed to partially fail if directory creation fails, but we
  // want to have as many of the specified tracked directories created as
  // possible.
  bool result = true;
  for (int index = 0; index < serialized.tracked_subdirectories_size();
       ++index) {
    const std::string& subdir = serialized.tracked_subdirectories(index);
    if (subdir.find("..") != std::string::npos) {
      result = false;
      continue;
    }
    FilePath directory = vault_path.Append(subdir);
    if (!file_util::DirectoryExists(directory)) {
      file_util::CreateDirectory(directory);
      if (set_vault_ownership_) {
        if (!platform_->SetOwnership(directory.value(), default_user_,
                                     default_group_)) {
          LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                     << default_group_ << ") of tracked directory path: "
                     << directory.value();
          file_util::Delete(directory, true);
          result = false;
        }
      }
    }
  }

  // Restore the umask
  platform_->SetMask(original_mask);
  return result;
}

bool Mount::ReplaceTrackedSubdirectories(
      const std::vector<std::string>& tracked_subdirectories,
      SerializedVaultKeyset* serialized) const {
  std::set<std::string> existing;
  for (int index = 0; index < serialized->tracked_subdirectories_size();
       ++index) {
    existing.insert(serialized->tracked_subdirectories(index));
  }
  bool new_exists = false;
  for (std::vector<std::string>::const_iterator itr =
       tracked_subdirectories.begin();
       itr != tracked_subdirectories.end();
       ++itr) {
    if (!existing.erase(*itr)) {
      new_exists = true;
    }
  }
  // If there are any subdirectories that were in one set but not the other,
  // then we need to replace
  if (existing.size() || new_exists) {
    serialized->clear_tracked_subdirectories();
    for (std::vector<std::string>::const_iterator itr =
         tracked_subdirectories.begin();
         itr != tracked_subdirectories.end();
         ++itr) {
      serialized->add_tracked_subdirectories(*itr);
    }
    return true;
  }
  return false;
}

void Mount::CleanUnmountedTrackedSubdirectories() const {
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
    FilePath vault_path = next_path.Append("vault");
    if (!file_util::DirectoryExists(vault_path)) {
      continue;
    }
    if (platform_->IsDirectoryMountedWith(home_dir_, vault_path.value())) {
      continue;
    }
    file_util::FileEnumerator subdir_enumerator(
        vault_path,
        false,
        file_util::FileEnumerator::DIRECTORIES);
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
  bool result = UnwrapVaultKeyset(credentials, false, &vault_keyset,
                                  &serialized, &mount_error);
  // Retry once if there is a TPM communications failure
  if (!result && mount_error == MOUNT_ERROR_TPM_COMM_ERROR) {
    result = UnwrapVaultKeyset(credentials, false, &vault_keyset,
                               &serialized, &mount_error);
  }
  return result;
}

bool Mount::LoadVaultKeyset(const Credentials& credentials,
                            SerializedVaultKeyset* serialized) const {
  // Load the encrypted keyset
  FilePath user_key_file(GetUserKeyFile(credentials));
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
  SecureBlob final_blob(serialized.ByteSize());
  serialized.SerializeWithCachedSizesToArray(
      static_cast<google::protobuf::uint8*>(final_blob.data()));
  unsigned int data_written = file_util::WriteFile(
      FilePath(GetUserKeyFile(credentials)),
      static_cast<const char*>(final_blob.const_data()),
      final_blob.size());

  if (data_written != final_blob.size()) {
    return false;
  }
  return true;
}

bool Mount::UnwrapVaultKeyset(const Credentials& credentials,
                              bool migrate_if_needed,
                              VaultKeyset* vault_keyset,
                              SerializedVaultKeyset* serialized,
                              MountError* error) const {
  FilePath salt_path(GetUserSaltFile(credentials));
  // If a separate salt file exists, it is the old-style keyset
  if (file_util::PathExists(salt_path)) {
    VaultKeyset old_keyset;
    if (!UnwrapVaultKeysetOld(credentials, &old_keyset, error)) {
      return false;
    }
    if (migrate_if_needed) {
      // This is not considered a fatal error.  Re-saving with the desired
      // protection is ideal, but not required.
      RewrapVaultKeyset(credentials, old_keyset, serialized);
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

    // Attempt to unwrap the master key with the passkey
    unsigned int wrap_flags = 0;
    Crypto::CryptoError crypto_error = Crypto::CE_NONE;
    if (!crypto_->UnwrapVaultKeyset(*serialized, passkey, &wrap_flags,
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
      bool tpm_wrapped = (wrap_flags & SerializedVaultKeyset::TPM_WRAPPED) != 0;
      bool scrypt_wrapped =
          (wrap_flags & SerializedVaultKeyset::SCRYPT_WRAPPED) != 0;
      bool should_tpm = (crypto_->has_tpm() && use_tpm_);
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
        if (RewrapVaultKeyset(credentials, *vault_keyset, &new_serialized)) {
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

  // Wrap the vault keyset
  SecureBlob salt(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  crypto_->GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                           salt.size());

  if (!crypto_->WrapVaultKeyset(vault_keyset, passkey, salt, serialized)) {
    LOG(ERROR) << "Wrapping vault keyset failed";
    return false;
  }

  return true;
}

bool Mount::RewrapVaultKeyset(const Credentials& credentials,
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
  // Attempt to unwrap the vault keyset with the specified credentials
  MountError mount_error;
  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  bool result = UnwrapVaultKeyset(old_credentials, false, &vault_keyset,
                                  &serialized, &mount_error);
  // Retry once if there is a TPM communications failure
  if (!result && mount_error == MOUNT_ERROR_TPM_COMM_ERROR) {
    result = UnwrapVaultKeyset(old_credentials, false, &vault_keyset,
                               &serialized, &mount_error);
  }
  if (result) {
    if (!RewrapVaultKeyset(credentials, vault_keyset, &serialized)) {
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
  if (!platform_->Mount("guestfs", home_dir_, "tmpfs", "")) {
    LOG(ERROR) << "Cryptohome mount failed: " << errno << " for guestfs";
    return false;
  }
  if (set_vault_ownership_) {
    if (!platform_->SetOwnership(home_dir_, default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of guestfs path: "
                 << home_dir_.c_str();
      bool was_busy;
      platform_->Unmount(home_dir_.c_str(), false, &was_busy);
      return false;
    }
  }
  CopySkeleton();
  return true;
}

string Mount::GetUserDirectory(const Credentials& credentials) const {
  return StringPrintf("%s/%s",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str());
}

string Mount::GetUserSaltFile(const Credentials& credentials) const {
  return StringPrintf("%s/%s/master.0.salt",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str());
}

string Mount::GetUserKeyFile(const Credentials& credentials) const {
  return StringPrintf("%s/%s/master.0",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str());
}

string Mount::GetUserVaultPath(const Credentials& credentials) const {
  return StringPrintf("%s/%s/vault",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str());
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
  // Get the vault keyset wrapper
  SecureBlob user_salt;
  GetUserSalt(credentials, true, &user_salt);
  SecureBlob passkey;
  credentials.GetPasskey(&passkey);
  SecureBlob passkey_wrapper;
  crypto_->PasskeyToWrapper(passkey, user_salt, 1, &passkey_wrapper);

  // Wrap the vault keyset
  SecureBlob salt(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  SecureBlob cipher_text;
  crypto_->GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                           salt.size());
  if (!crypto_->WrapVaultKeysetOld(vault_keyset, passkey_wrapper, salt,
                                   &cipher_text)) {
    LOG(ERROR) << "Wrapping vault keyset failed";
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

bool Mount::UnwrapVaultKeysetOld(const Credentials& credentials,
                                 VaultKeyset* vault_keyset,
                                 MountError* error) const {
  // Generate the passkey wrapper (key encryption key)
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
  SecureBlob passkey_wrapper;
  crypto_->PasskeyToWrapper(passkey, user_salt, 1, &passkey_wrapper);

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

  // Attempt to unwrap the master key with the passkey wrapper
  if (!crypto_->UnwrapVaultKeysetOld(cipher_text, passkey_wrapper,
                                     vault_keyset)) {
    if (error) {
      *error = Mount::MOUNT_ERROR_KEY_FAILURE;
    }
    return false;
  }

  return true;
}

} // namespace cryptohome
