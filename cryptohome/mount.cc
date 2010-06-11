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
#include <chromeos/utility.h>

#include "crypto.h"
#include "cryptohome_common.h"
#include "platform.h"
#include "username_passkey.h"

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
      platform_(default_platform_.get()) {
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

  // One-time load of the global system salt (used in generating username
  // hashes)
  if (!LoadFileBytes(FilePath(StringPrintf("%s/salt", shadow_root_.c_str())),
                       &system_salt_)) {
    result = false;
  }

  return result;
}

bool Mount::EnsureCryptohome(const Credentials& credentials, bool* created) {
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
    bool result = CreateCryptohome(credentials, 0);
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

bool Mount::MountCryptohome(const Credentials& credentials, int index,
                            MountError* mount_error) {
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
  if (!UnwrapVaultKeyset(credentials, index, &vault_keyset, mount_error)) {
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
                                         ",ecryptfs_fnek_sig=%s,ecryptfs_sig=%s"
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
  return true;
}

bool Mount::UnmountCryptohome() {
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

bool Mount::RemoveCryptohome(const Credentials& credentials) {
  std::string user_dir = GetUserDirectory(credentials);
  CHECK(user_dir.length() > (shadow_root_.length() + 1));

  return file_util::Delete(FilePath(user_dir), true);
}

bool Mount::IsCryptohomeMounted() {
  return platform_->IsDirectoryMounted(home_dir_);
}

bool Mount::IsCryptohomeMountedForUser(const Credentials& credentials) {
  return platform_->IsDirectoryMountedWith(home_dir_,
                                           GetUserVaultPath(credentials));
}

bool Mount::CreateCryptohome(const Credentials& credentials, int index) {
  int original_mask = platform_->SetMask(kDefaultUmask);

  // Create the user's entry in the shadow root
  FilePath user_dir(GetUserDirectory(credentials));
  file_util::CreateDirectory(user_dir);

  // Generates a new master key and salt at the given index
  if (!CreateMasterKey(credentials, index)) {
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

bool Mount::TestCredentials(const Credentials& credentials) {
  // Iterate over the keys in the user's entry in the shadow root to see if
  // these credentials successfully decrypt any
  for (int index = 0; /* loop forever */; index++) {
    MountError mount_error;
    VaultKeyset vault_keyset;
    if (UnwrapVaultKeyset(credentials, index, &vault_keyset, &mount_error)) {
      return true;
    } else if (mount_error != Mount::MOUNT_ERROR_KEY_FAILURE) {
      break;
    }
  }
  return false;
}

bool Mount::UnwrapVaultKeyset(const Credentials& credentials, int index,
                              VaultKeyset* vault_keyset, MountError* error) {
  // Generate the passkey wrapper (key encryption key)
  SecureBlob user_salt;
  GetUserSalt(credentials, index, false, &user_salt);
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
  FilePath user_key_file(GetUserKeyFile(credentials, index));
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

  // Attempt to unwrap the master key at the index with the passkey wrapper
  if (!crypto_->UnwrapVaultKeyset(cipher_text, passkey_wrapper, vault_keyset)) {
    if (error) {
      *error = Mount::MOUNT_ERROR_KEY_FAILURE;
    }
    return false;
  }

  return true;
}

bool Mount::MigratePasskey(const Credentials& credentials,
                           const char* old_key) {
  // Iterate over the keys in the user's entry in the shadow root to see if
  // these credentials successfully decrypt any
  std::string username = credentials.GetFullUsernameString();
  UsernamePasskey old_credentials(username.c_str(),
                                  SecureBlob(old_key, strlen(old_key)));
  for (int index = 0; /* loop forever */; index++) {
    // Attempt to unwrap the vault keyset with the specified credentials
    MountError mount_error;
    VaultKeyset vault_keyset;
    if (UnwrapVaultKeyset(old_credentials, index, &vault_keyset,
                          &mount_error)) {
      // Save to the next key index first so that if there is a failure, we
      // don't delete the existing working keyset.
      bool save_result = SaveVaultKeyset(credentials, vault_keyset, index + 1);
      if (save_result) {
        // Saved okay, move to index 0.
        if (!file_util::ReplaceFile(FilePath(GetUserKeyFile(credentials,
                                                           index + 1)),
                                   FilePath(GetUserKeyFile(credentials, 0)))) {
          return false;
        }
        if (!file_util::ReplaceFile(FilePath(GetUserSaltFile(credentials,
                                                           index + 1)),
                                   FilePath(GetUserSaltFile(credentials, 0)))) {
          return false;
        }
        // Remove older keys
        for (index = 1; /* loop forever */; index++) {
          FilePath old_user_key_file(GetUserKeyFile(old_credentials, index));
          FilePath old_user_salt_file(GetUserSaltFile(old_credentials, index));
          if (!file_util::AbsolutePath(&old_user_key_file)) {
            break;
          }
          file_util::Delete(old_user_key_file, false);
          file_util::Delete(old_user_salt_file, false);
        }
        return true;
      } else {
        // Couldn't save the vault keyset, delete it
        file_util::Delete(FilePath(GetUserKeyFile(credentials, index + 1)),
                          false);
        file_util::Delete(FilePath(GetUserSaltFile(credentials, index + 1)),
                          false);
        return false;
      }
    } else if (mount_error != Mount::MOUNT_ERROR_KEY_FAILURE) {
      break;
    }
  }
  return false;
}

bool Mount::MountGuestCryptohome() {
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

string Mount::GetUserDirectory(const Credentials& credentials) {
  return StringPrintf("%s/%s",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str());
}

string Mount::GetUserSaltFile(const Credentials& credentials, int index) {
  return StringPrintf("%s/%s/master.%d.salt",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str(),
                      index);
}

string Mount::GetUserKeyFile(const Credentials& credentials, int index) {
  return StringPrintf("%s/%s/master.%d",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str(),
                      index);
}

string Mount::GetUserVaultPath(const Credentials& credentials) {
  return StringPrintf("%s/%s/vault",
                      shadow_root_.c_str(),
                      credentials.GetObfuscatedUsername(system_salt_).c_str());
}

void Mount::RecursiveCopy(const FilePath& destination,
                          const FilePath& source) {
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

void Mount::CopySkeletonForUser(const Credentials& credentials) {
  if (IsCryptohomeMountedForUser(credentials)) {
    CopySkeleton();
  }
}

void Mount::CopySkeleton() {
  RecursiveCopy(FilePath(home_dir_), FilePath(skel_source_));
}

void Mount::GetSecureRandom(unsigned char *rand, int length) const {
  crypto_->GetSecureRandom(rand, length);
}

bool Mount::CreateMasterKey(const Credentials& credentials, int index) {
  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(*this);
  return SaveVaultKeyset(credentials, vault_keyset, index);
}

bool Mount::SaveVaultKeyset(const Credentials& credentials,
                            const VaultKeyset& vault_keyset,
                            int index) {
  // Get the vault keyset wrapper
  SecureBlob user_salt;
  GetUserSalt(credentials, index, true, &user_salt);
  SecureBlob passkey;
  credentials.GetPasskey(&passkey);
  SecureBlob passkey_wrapper;
  crypto_->PasskeyToWrapper(passkey, user_salt, 1, &passkey_wrapper);

  // Wrap the vault keyset
  SecureBlob salt(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  SecureBlob cipher_text;
  crypto_->GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                           salt.size());
  if (!crypto_->WrapVaultKeyset(vault_keyset, passkey_wrapper, salt,
                               &cipher_text)) {
    LOG(ERROR) << "Wrapping vault keyset failed";
    return false;
  }

  // Save the master key
  unsigned int data_written = file_util::WriteFile(
      FilePath(GetUserKeyFile(credentials, index)),
      static_cast<const char*>(cipher_text.const_data()),
      cipher_text.size());

  if (data_written != cipher_text.size()) {
    LOG(ERROR) << "Write to master key failed";
    return false;
  }
  return true;
}

void Mount::GetSystemSalt(chromeos::Blob* salt) {
  *salt = system_salt_;
}

void Mount::GetUserSalt(const Credentials& credentials, int index, bool force,
                        SecureBlob* salt) {
  FilePath path(GetUserSaltFile(credentials, index));
  crypto_->GetOrCreateSalt(path, CRYPTOHOME_DEFAULT_SALT_LENGTH, force, salt);
}

bool Mount::LoadFileBytes(const FilePath& path,
                          SecureBlob* blob) {
  int64 file_size;
  if (!file_util::PathExists(path)) {
    LOG(INFO) << path.value() << " does not exist!";
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

} // namespace cryptohome
