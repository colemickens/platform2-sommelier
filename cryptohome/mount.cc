// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

#include "mount.h"

#include <errno.h>

#include <base/file_util.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/platform_thread.h>
#include <base/time.h>
#include <base/string_util.h>
#include <base/values.h>
#include <chromeos/utility.h>

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
      process_config_(false),
      default_current_user_(new UserSession()),
      current_user_(default_current_user_.get()) {
}

Mount::~Mount() {
}

bool Mount::Init() {
  bool result = true;

  if (process_config_) {
    LoadConfigurationFile();
  }

  // Get the user id and group id of the default user
  if (!platform_->GetUserId(default_username_, &default_user_,
                           &default_group_)) {
    result = false;
  }

  // One-time load of the global system salt (used in generating username
  // hashes)
  if (!LoadFileBytes(FilePath(StringPrintf("%s/salt", shadow_root_.c_str())),
                       &system_salt_)) {
    result = false;
  }

  crypto_->set_use_tpm(use_tpm_);
  crypto_->set_fallback_to_scrypt(fallback_to_scrypt_);

  if (!crypto_->Init()) {
    result = false;
  }

  current_user_->Init(crypto_);

  return result;
}

void Mount::LoadConfigurationFile() {
  // Attempt to load configuration
  string str_config;
  if (LoadFileString(FilePath(StringPrintf("%s/cryptohome.json",
                                           shadow_root_.c_str())),
                     &str_config)) {
    std::string json_error;
    Value* json_config = base::JSONReader::ReadAndReturnError(str_config,
                                                              true,
                                                              &json_error);
    if (json_config) {
      if (json_config->GetType() == Value::TYPE_DICTIONARY) {
        DictionaryValue* dict_config =
            static_cast<DictionaryValue*>(json_config);
        std::wstring key = L"use_tpm";
        bool local_use_tpm = false;
        if (dict_config->GetBoolean(key, &local_use_tpm)) {
          use_tpm_ = local_use_tpm;
        }
        key = L"fallback_to_scrypt";
        bool local_fallback_to_scrypt = false;
        if (dict_config->GetBoolean(key, &local_fallback_to_scrypt)) {
          fallback_to_scrypt_ = local_fallback_to_scrypt;
        }
      }
      delete json_config;
    } else {
      LOG(ERROR) << "Malformed config JSON: " << json_error;
    }
  }
}

bool Mount::EnsureCryptohome(const Credentials& credentials,
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

bool Mount::MountCryptohome(const Credentials& credentials,
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

  bool created = false;
  if (!EnsureCryptohome(credentials, &created)) {
    LOG(ERROR) << "Error creating cryptohome.";
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  // Attempt to unwrap the vault keyset with the specified credentials
  VaultKeyset vault_keyset;
  if (!UnwrapVaultKeyset(credentials, true, &vault_keyset, mount_error)) {
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
                                         CRYPTOHOME_AES_KEY_BYTES,
                                         fnek_signature.c_str(),
                                         key_signature.c_str());

  // Mount cryptohome
  string vault_path = GetUserVaultPath(credentials);
  if (!platform_->Mount(vault_path, home_dir_, "ecryptfs", ecryptfs_options)) {
    LOG(INFO) << "Cryptohome mount failed: " << errno << " for vault: "
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
    if (was_busy) {
      // Signal processes to close
      if (platform_->TerminatePidsWithOpenFiles(home_dir_, false)) {
        // Then we had to send a SIGINT to some processes with open files.  Give
        // a "grace" period before killing with a SIGKILL.
        // TODO(fes): This isn't ideal, nor is it accurate (a static sleep can't
        // guarantee that processes will clean up in time).  If we switch to VFS
        // namespace-based mounts, then we can get away from this construct.
        PlatformThread::Sleep(100);
        sync();
        if (platform_->TerminatePidsWithOpenFiles(home_dir_, true)) {
          PlatformThread::Sleep(100);
        }
      }
    }
    // Failed to unmount immediately, do a lazy unmount
    platform_->Unmount(home_dir_, true, NULL);
    sync();
    // TODO(fes): This should return an error condition if it is still mounted.
    // Right now it doesn't, since it should get cleaned up outside of
    // cryptohome since we failed over to a lazy unmount
  }

  // TODO(fes): Do we need to keep this behavior?
  //TerminatePidsForUser(default_user_, true);

  // Clear the user keyring if the unmount was successful
  crypto_->ClearKeyset();

  return true;
}

bool Mount::RemoveCryptohome(const Credentials& credentials) const {
  std::string user_dir = GetUserDirectory(credentials);
  CHECK(user_dir.length() > (shadow_root_.length() + 1));

  return file_util::Delete(FilePath(user_dir), true);
}

bool Mount::IsCryptohomeMounted() const {
  return platform_->IsDirectoryMounted(home_dir_);
}

bool Mount::IsCryptohomeMountedForUser(const Credentials& credentials) const {
  return platform_->IsDirectoryMountedWith(home_dir_,
                                           GetUserVaultPath(credentials));
}

bool Mount::CreateCryptohome(const Credentials& credentials) const {
  int original_mask = platform_->SetMask(kDefaultUmask);

  // Create the user's entry in the shadow root
  FilePath user_dir(GetUserDirectory(credentials));
  file_util::CreateDirectory(user_dir);

  // Generates a new master key
  if (!CreateMasterKey(credentials)) {
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

  // Restore the umask
  platform_->SetMask(original_mask);
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
  return UnwrapVaultKeyset(credentials, false, &vault_keyset, &mount_error);
}

bool Mount::UnwrapVaultKeyset(const Credentials& credentials,
                              bool migrate_if_needed,
                              VaultKeyset* vault_keyset,
                              MountError* error) const {
  FilePath salt_path(GetUserSaltFile(credentials));
  // If a separate salt file exists, it is the old-style keyset
  if (file_util::PathExists(salt_path)) {
    VaultKeyset old_keyset;
    if (!UnwrapVaultKeysetOld(credentials, &old_keyset, error)) {
      return false;
    }
    if (migrate_if_needed) {
      // TODO(fes): This is not (right now) a fatal error
      ResaveVaultKeyset(credentials, old_keyset);
    }
    vault_keyset->FromVaultKeyset(old_keyset);
    return true;
  } else {
    SecureBlob passkey;
    credentials.GetPasskey(&passkey);

    // Load the encrypted keyset
    FilePath user_key_file(GetUserKeyFile(credentials));
    if (!file_util::PathExists(user_key_file)) {
      if (error) {
        *error = MOUNT_ERROR_NO_SUCH_FILE;
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

    // Attempt to unwrap the master key with the passkey
    int wrap_flags = 0;
    if (!crypto_->UnwrapVaultKeyset(cipher_text, passkey, &wrap_flags,
                                    vault_keyset)) {
      if (error) {
        *error = Mount::MOUNT_ERROR_KEY_FAILURE;
      }
      return false;
    }

    // Calling EnsureTpm here handles the case where a user logged in while
    // cryptohome was taking TPM ownership.  In that case, their vault keyset
    // would be scrypt-wrapped and the TPM would not be connected.  If we're
    // configured to use the TPM, calling EnsureTpm will try to connect, and if
    // successful, the call to has_tpm() below will succeed, allowing
    // re-wrapping (migration) using the TPM.
    if (use_tpm_) {
      crypto_->EnsureTpm();
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
      if (tpm_wrapped && !scrypt_wrapped && should_tpm)
        break; // 5, 8 (but remove scrypt if it is stacked)
      if (scrypt_wrapped && should_scrypt && !should_tpm)
        break; // 12
      if (!tpm_wrapped && !scrypt_wrapped && !should_tpm && !should_scrypt)
        break; // 1
      // TODO(fes): This is not (right now) a fatal error
      ResaveVaultKeyset(credentials, *vault_keyset);
    } while(false);

    return true;
  }
}

bool Mount::SaveVaultKeyset(const Credentials& credentials,
                            const VaultKeyset& vault_keyset) const {
  // We don't do passkey to wrapper conversion because it is salted during save
  SecureBlob passkey;
  credentials.GetPasskey(&passkey);

  // Wrap the vault keyset
  SecureBlob salt(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  crypto_->GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                           salt.size());

  SecureBlob cipher_text;
  if (!crypto_->WrapVaultKeyset(vault_keyset, passkey, salt, &cipher_text)) {
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

bool Mount::ResaveVaultKeyset(const Credentials& credentials,
                              const VaultKeyset& vault_keyset) const {
  std::vector<std::string> files(2);
  files[0] = GetUserSaltFile(credentials);
  files[1] = GetUserKeyFile(credentials);
  if (!CacheOldFiles(credentials, files)) {
    LOG(ERROR) << "Couldn't cache old key material.";
    return false;
  }
  if (!SaveVaultKeyset(credentials, vault_keyset)) {
    LOG(ERROR) << "Couldn't save keyset.";
    RevertCacheFiles(credentials, files);
    return false;
  }
  DeleteCacheFiles(credentials, files);
  return true;
}

bool Mount::MigratePasskey(const Credentials& credentials,
                           const char* old_key) const {
  current_user_->Reset();

  std::string username = credentials.GetFullUsernameString();
  UsernamePasskey old_credentials(username.c_str(),
                                  SecureBlob(old_key, strlen(old_key)));
  // Attempt to unwrap the vault keyset with the specified credentials
  MountError mount_error;
  VaultKeyset vault_keyset;
  if (UnwrapVaultKeyset(old_credentials, false, &vault_keyset,
                        &mount_error)) {
    if (!ResaveVaultKeyset(credentials, vault_keyset)) {
      LOG(ERROR) << "Couldn't save keyset.";
      return false;
    }
    current_user_->SetUser(credentials);
    return true;
  }
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

void Mount::GetSecureRandom(unsigned char *rand, int length) const {
  crypto_->GetSecureRandom(rand, length);
}

bool Mount::CreateMasterKey(const Credentials& credentials) const {
  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(*this);
  return SaveVaultKeyset(credentials, vault_keyset);
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
  for (unsigned int index = 0; index < files.size(); index++) {
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
  for (unsigned int index = 0; index < files.size(); index++) {
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
  for (unsigned int index = 0; index < files.size(); index++) {
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
  if (file_size > INT_MAX) {
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
      *error = MOUNT_ERROR_NO_SUCH_FILE;
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
