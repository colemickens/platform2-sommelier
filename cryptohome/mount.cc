// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

// TODO(fes): Use correct ordering of file includes once the platform-specific
// calls are moved into a separate file.  Right now, this is required in order
// to avoid redefinitions.
#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/time.h"
#include "base/string_util.h"
#include "chromeos/utility.h"
#include "cryptohome/cryptohome_common.h"
#include "cryptohome/mount.h"
#include "cryptohome/username_passkey.h"

extern "C" {
#include <ecryptfs.h>
#include <keyutils.h>
}
#include <dirent.h>
#include <errno.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <pwd.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

using std::string;

namespace cryptohome {

const string kDefaultEntropySource = "/dev/urandom";
const string kDefaultHomeDir = "/home/chronos/user";
const int kDefaultMountOptions = MS_NOEXEC | MS_NOSUID | MS_NODEV;
const string kDefaultShadowRoot = "/home/.shadow";
const string kDefaultSharedUser = "chronos";
const string kDefaultSkeletonSource = "/etc/skel";
const string kIncognitoUser = "incognito";
const string kMtab = "/etc/mtab";
const string kOpenSSLMagic = "Salted__";
const std::string kProcDir = "/proc";

Mount::Mount()
    : default_user_(-1),
      default_group_(-1),
      default_username_(kDefaultSharedUser),
      entropy_source_(kDefaultEntropySource),
      home_dir_(kDefaultHomeDir),
      shadow_root_(kDefaultShadowRoot),
      skel_source_(kDefaultSkeletonSource),
      system_salt_(),
      set_vault_ownership_(true) {
}

Mount::Mount(const std::string& username, const std::string& entropy_source,
        const std::string& home_dir, const std::string& shadow_root,
        const std::string& skel_source)
    : default_user_(-1),
      default_group_(-1),
      default_username_(username),
      entropy_source_(entropy_source),
      home_dir_(home_dir),
      shadow_root_(shadow_root),
      skel_source_(skel_source),
      system_salt_(),
      set_vault_ownership_(true) {
}

Mount::~Mount() {
}

bool Mount::Init() {
  bool result = true;

  // Load the passwd entry for the shared user
  struct passwd* user_info = getpwnam(default_username_.c_str());
  if (user_info) {
    // Store the user's uid/gid for later use in changing vault ownership
    default_user_ = user_info->pw_uid;
    default_group_ = user_info->pw_gid;
  } else {
    result = false;
  }

  // One-time load of the global system salt (used in generating username
  // hashes)
  if (!LoadFileBytes(FilePath(StringPrintf("%s/salt", shadow_root_.c_str())),
                       system_salt_)) {
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
  std::string username = credentials.GetFullUsername();
  if (username.compare(kIncognitoUser) == 0) {
    // TODO(fes): Have incognito set error conditions?
    if (mount_error) {
      *mount_error = MOUNT_ERROR_NONE;
    }
    return MountIncognitoCryptohome();
  }

  bool created = false;
  if (!EnsureCryptohome(credentials, &created)) {
    LOG(ERROR) << "Error creating cryptohome.";
    if (mount_error) {
      *mount_error = MOUNT_ERROR_FATAL;
    }
    return false;
  }

  FilePath user_key_file(GetUserKeyFile(credentials, index));

  // Retrieve the user's salt for the key index
  SecureBlob user_salt = GetUserSalt(credentials, index);

  // Generate the passkey wrapper (key encryption key)
  SecureBlob passkey_wrapper =
      PasskeyToWrapper(credentials.GetPasskey(), user_salt, 1);

  // Attempt to unwrap the master key at the index with the passkey wrapper
  VaultKeyset vault_keyset;
  bool mount_result = UnwrapMasterKey(user_key_file, passkey_wrapper,
                                      &vault_keyset);

  if (!mount_result) {
    if (mount_error) {
      *mount_error = Mount::MOUNT_ERROR_KEY_FAILURE;
    }
    return false;
  }

  // Add the decrypted key to the keyring so that ecryptfs can use it
  string key_signature, fnek_signature;
  if (!AddKeyToEcryptfsKeyring(vault_keyset, &key_signature, &fnek_signature)) {
    LOG(ERROR) << "Cryptohome mount failed because of keyring failure.";
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

  // TODO(fes): mount(1) -> mount(2): how to issue "user"
  string vault_path = GetUserVaultPath(credentials);
  // Attempt to mount the user's cryptohome
  if (mount(vault_path.c_str(), home_dir_.c_str(),
            "ecryptfs", kDefaultMountOptions, ecryptfs_options.c_str())) {
    LOG(ERROR) << "Cryptohome mount failed: " << errno << " for vault: "
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
  if (!Unmount(home_dir_.c_str(), false, &was_busy)) {
    // If the unmount fails, do a lazy unmount
    Unmount(home_dir_.c_str(), true, NULL);
    if (was_busy) {
      // Signal processes to close
      if (TerminatePidsWithOpenFiles(home_dir_, false)) {
        // Then we had to send a SIGINT to some processes with open files.  Give
        // a "grace" period before killing with a SIGKILL.
        // TODO(fes): This isn't ideal, nor is it accurate (a static sleep can't
        // guarantee that processes will clean up in time).  If we switch to VFS
        // namespace-based mounts, then we can get away from this construct.
        PlatformThread::Sleep(100);
        sync();
        if (TerminatePidsWithOpenFiles(home_dir_, true)) {
          PlatformThread::Sleep(100);
        }
      }
    }
    sync();
    // TODO(fes): This should return an error condition if it is still mounted.
    // Right now it doesn't, since it should get cleaned up outside of
    // cryptohome since we failed over to a lazy unmount
  }

  // TODO(fes): Do we need to keep this behavior?
  //TerminatePidsForUser(default_user_, true);

  // Clear the user keyring if the unmount was successful
  keyctl(KEYCTL_CLEAR, KEY_SPEC_USER_KEYRING);

  return true;
}

bool Mount::RemoveCryptohome(const Credentials& credentials) {
  std::string user_dir = GetUserDirectory(credentials);
  CHECK(user_dir.length() > (shadow_root_.length() + 1));

  return file_util::Delete(FilePath(user_dir), true);
}

bool Mount::IsCryptohomeMounted() {
  // Trivial string match from /etc/mtab to see if the cryptohome mount point is
  // listed.  This works because Chrome OS is a controlled environment and the
  // only way /home/chronos/user should be mounted is if cryptohome mounted it.
  string contents;
  if (file_util::ReadFileToString(FilePath(kMtab), &contents)) {
    if (contents.find(StringPrintf(" %s ", home_dir_.c_str()).c_str())
        != string::npos) {
      return true;
    }
  }
  return false;
}

bool Mount::IsCryptohomeMountedForUser(const Credentials& credentials) {
  // Trivial string match from /etc/mtab to see if the cryptohome mount point
  // and the user's vault path are present.  Assumes this user is mounted if it
  // finds both.  This will need to change if simultaneous login is implemented.
  string contents;
  if (file_util::ReadFileToString(FilePath(kMtab), &contents)) {
    FilePath vault_path(GetUserVaultPath(credentials));
    if ((contents.find(StringPrintf(" %s ", home_dir_.c_str()).c_str())
         != string::npos)
        && (contents.find(StringPrintf("%s ",
                                       vault_path.value().c_str()).c_str())
            != string::npos)) {
      return true;
    }
  }
  return false;
}

bool Mount::CreateCryptohome(const Credentials& credentials, int index) {
  // Save the current umask
  mode_t original_mask = umask(S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH
                               | S_IXOTH);

  // Create the user's entry in the shadow root
  FilePath user_dir(GetUserDirectory(credentials));
  file_util::CreateDirectory(user_dir);

  // Generates a new master key and salt at the given index
  if (!CreateMasterKey(credentials, index)) {
    umask(original_mask);
    return false;
  }

  // Create the user's path and set the proper ownership
  FilePath vault_path(GetUserVaultPath(credentials));
  if (!file_util::CreateDirectory(vault_path)) {
    LOG(ERROR) << "Couldn't create vault path: " << vault_path.value().c_str();
    umask(original_mask);
    return false;
  }
  if (set_vault_ownership_) {
    // TODO(fes): Move platform-specific calls to a separate class
    if (chown(vault_path.value().c_str(), default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of vault path: "
                 << vault_path.value().c_str();
      umask(original_mask);
      return false;
    }
  }

  // Restore the umask
  umask(original_mask);
  return true;
}

bool Mount::TestCredentials(const Credentials& credentials) {
  // Iterate over the keys in the user's entry in the shadow root to see if
  // these credentials successfully decrypt any
  for (int index = 0; /* loop forever */; index++) {
    FilePath user_key_file(GetUserKeyFile(credentials, index));
    if (!file_util::AbsolutePath(&user_key_file)) {
      break;
    }

    SecureBlob user_salt = GetUserSalt(credentials, index);

    SecureBlob passkey_wrapper = PasskeyToWrapper(credentials.GetPasskey(),
                                                  user_salt, 1);

    VaultKeyset vault_keyset;
    if (UnwrapMasterKey(user_key_file, passkey_wrapper, &vault_keyset)) {
      return true;
    }
  }
  return false;
}

bool Mount::MigratePasskey(const Credentials& credentials,
                           const char* old_key) {
  // Iterate over the keys in the user's entry in the shadow root to see if
  // these credentials successfully decrypt any
  std::string username = credentials.GetFullUsername();
  UsernamePasskey old_credentials(username.c_str(), username.length(),
                                  old_key, strlen(old_key));
  for (int index = 0; /* loop forever */; index++) {
    FilePath user_key_file(GetUserKeyFile(old_credentials, index));
    if (!file_util::AbsolutePath(&user_key_file)) {
      break;
    }

    SecureBlob user_salt = GetUserSalt(old_credentials, index);

    SecureBlob passkey_wrapper = PasskeyToWrapper(old_credentials.GetPasskey(),
                                                  user_salt, 1);

    VaultKeyset vault_keyset;
    if (UnwrapMasterKey(user_key_file, passkey_wrapper, &vault_keyset)) {
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
    }
  }
  return false;
}

bool Mount::MountIncognitoCryptohome() {
  // Attempt to mount incognitofs
  if (mount("incognitofs", home_dir_.c_str(), "tmpfs",
            kDefaultMountOptions, "")) {
    LOG(ERROR) << "Cryptohome mount failed: " << errno << " for incognitofs";
    return false;
  }
  if (set_vault_ownership_) {
    if (chown(home_dir_.c_str(), default_user_, default_group_)) {
      LOG(ERROR) << "Couldn't change owner (" << default_user_ << ":"
                 << default_group_ << ") of incognitofs path: "
                 << home_dir_.c_str();
      bool was_busy;
      Unmount(home_dir_.c_str(), false, &was_busy);
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
  // TODO(fes): Get assistance from the TPM when it is available
  // Seed the OpenSSL random number generator until it is happy
  while (!RAND_status()) {
    char buffer[256];
    file_util::ReadFile(FilePath(entropy_source_), buffer, sizeof(buffer));
    RAND_add(buffer, sizeof(buffer), sizeof(buffer));
  }

  // Have OpenSSL generate the random bytes
  RAND_bytes(rand, length);
}

bool Mount::Unmount(const std::string& path, bool lazy, bool* was_busy) {
  if (lazy) {
    // TODO(fes): Place platform-specific calls in a separate class so that
    // they can be mocked.
    if (umount2(path.c_str(), MNT_DETACH)) {
      if (was_busy) {
        *was_busy = (errno == EBUSY);
      }
      return false;
    }
  } else {
    if (umount(path.c_str())) {
      if (was_busy) {
        *was_busy = (errno == EBUSY);
      }
      return false;
    }
  }
  if (was_busy) {
    *was_busy = false;
  }
  return true;
}

bool Mount::UnwrapMasterKey(const FilePath& path,
                            const chromeos::Blob& passkey,
                            VaultKeyset* vault_keyset) {
  // TODO(fes): Update this with openssl/tpm_engine or opencryptoki once
  // available
  // Load the encrypted master key
  SecureBlob cipher_text;
  if (!LoadFileBytes(path, cipher_text)) {
    LOG(ERROR) << "Unable to read master key file";
    return false;
  }

  unsigned int header_size = kOpenSSLMagic.length() + PKCS5_SALT_LEN;

  if (cipher_text.size() < header_size) {
    LOG(ERROR) << "Master key file too short";
    return false;
  }

  // Grab the salt used in converting the passkey to a key (OpenSSL
  // passkey-encrypted files have the format:
  // Salted__<8-byte-salt><ciphertext>)
  unsigned char salt[PKCS5_SALT_LEN];
  memcpy(salt, &cipher_text[kOpenSSLMagic.length()], PKCS5_SALT_LEN);

  cipher_text.erase(cipher_text.begin(), cipher_text.begin() + header_size);

  unsigned char wrapper_key[EVP_MAX_KEY_LENGTH];
  unsigned char iv[EVP_MAX_IV_LENGTH];

  // Convert the passkey to a key encryption key
  if (!EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, &passkey[0],
                          passkey.size(), 1, wrapper_key, iv)) {
    LOG(ERROR) << "Failure converting bytes to key";
    return false;
  }

  SecureBlob plain_text(cipher_text.size());

  int final_size = 0;
  int decrypt_size = plain_text.size();

  // Do the actual decryption
  EVP_CIPHER_CTX d_ctx;
  EVP_CIPHER_CTX_init(&d_ctx);
  EVP_DecryptInit_ex(&d_ctx, EVP_aes_256_ecb(), NULL, wrapper_key, iv);
  if (!EVP_DecryptUpdate(&d_ctx, &plain_text[0], &decrypt_size,
                        &cipher_text[0],
                        cipher_text.size())) {
    LOG(ERROR) << "DecryptUpdate failed";
    return false;
  }
  if (!EVP_DecryptFinal_ex(&d_ctx, &plain_text[decrypt_size], &final_size)) {
    unsigned long err = ERR_get_error();
    ERR_load_ERR_strings();
    ERR_load_crypto_strings();

    LOG(ERROR) << "DecryptFinal Error: " << err
               << ": " << ERR_lib_error_string(err)
               << ", " << ERR_func_error_string(err)
               << ", " << ERR_reason_error_string(err);

    return false;
  }
  final_size += decrypt_size;

  plain_text.resize(final_size);

  if (plain_text.size() != VaultKeyset::SerializedSize()) {
    LOG(ERROR) << "Plain text was not the correct size: " << plain_text.size()
               << ", expected: " << VaultKeyset::SerializedSize();
    return false;
  }

  (*vault_keyset).AssignBuffer(plain_text);

  return true;
}

bool Mount::CreateMasterKey(const Credentials& credentials, int index) {
  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(*this);
  return SaveVaultKeyset(credentials, vault_keyset, index);
}

bool Mount::SaveVaultKeyset(const Credentials& credentials,
                            const VaultKeyset& vault_keyset,
                            int index) {
  unsigned char wrapper_key[EVP_MAX_KEY_LENGTH];
  unsigned char iv[EVP_MAX_IV_LENGTH];
  unsigned char salt[PKCS5_SALT_LEN];

  // Create a salt for this master key
  GetSecureRandom(salt, sizeof(salt));
  SecureBlob user_salt = GetUserSalt(credentials, index, true);

  // Convert the passkey to a passkey wrapper
  SecureBlob passkey_wrapper =
      PasskeyToWrapper(credentials.GetPasskey(), user_salt, 1);

  // Convert the passkey wrapper to a key encryption key
  if (!EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, &passkey_wrapper[0],
                          passkey_wrapper.size(), 1, wrapper_key, iv)) {
    LOG(ERROR) << "Failure converting bytes to key";
    return false;
  }

  SecureBlob keyset_blob = vault_keyset.ToBuffer();

  // Store the salt and encrypt the master key
  unsigned int header_size = kOpenSSLMagic.length() + PKCS5_SALT_LEN;
  SecureBlob cipher_text(header_size
                         + keyset_blob.size()
                         + EVP_CIPHER_block_size(EVP_aes_256_ecb()));
  memcpy(&cipher_text[0], kOpenSSLMagic.c_str(), kOpenSSLMagic.length());
  memcpy(&cipher_text[kOpenSSLMagic.length()], salt, PKCS5_SALT_LEN);

  int current_size = header_size;
  int encrypt_size = 0;
  EVP_CIPHER_CTX e_ctx;
  EVP_CIPHER_CTX_init(&e_ctx);

  // Encrypt the keyset
  EVP_EncryptInit_ex(&e_ctx, EVP_aes_256_ecb(), NULL, wrapper_key, iv);
  if (!EVP_EncryptUpdate(&e_ctx, &cipher_text[current_size], &encrypt_size,
                         &keyset_blob[0],
                         keyset_blob.size())) {
    LOG(ERROR) << "EncryptUpdate failed";
    return false;
  }
  current_size += encrypt_size;
  encrypt_size = 0;

  // Finish the encryption
  if (!EVP_EncryptFinal_ex(&e_ctx, &cipher_text[current_size], &encrypt_size)) {
    LOG(ERROR) << "EncryptFinal failed";
    return false;
  }
  current_size += encrypt_size;
  cipher_text.resize(current_size);

  chromeos::SecureMemset(wrapper_key, sizeof(wrapper_key), 0);

  // Save the master key
  unsigned int data_written = file_util::WriteFile(
      FilePath(GetUserKeyFile(credentials, index)),
      reinterpret_cast<const char*>(&cipher_text[0]),
      cipher_text.size());

  if (data_written != cipher_text.size()) {
    LOG(ERROR) << "Write to master key failed";
    return false;
  }
  return true;
}

SecureBlob Mount::PasskeyToWrapper(const chromeos::Blob& passkey,
                              const chromeos::Blob& salt, int iters) {
  // TODO(fes): Update this when TPM support is available, or use a memory-
  // bound strengthening algorithm.
  int update_length = passkey.size();
  SecureBlob holder(CRYPTOHOME_MAX(update_length, SHA_DIGEST_LENGTH));
  memcpy(&holder[0], &passkey[0], update_length);

  // Repeatedly hash the user passkey and salt to generate the wrapper
  for (int i = 0; i < iters; ++i) {
    SHA_CTX ctx;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&ctx);
    SHA1_Update(&ctx, &salt[0], salt.size());
    SHA1_Update(&ctx, &holder[0], update_length);
    SHA1_Final(md_value, &ctx);

    memcpy(&holder[0], md_value, SHA_DIGEST_LENGTH);
    update_length = SHA_DIGEST_LENGTH;
  }

  holder.resize(update_length);
  SecureBlob wrapper(update_length * 2);
  AsciiEncodeToBuffer(holder, reinterpret_cast<char*>(&wrapper[0]),
                      wrapper.size());
  return wrapper;
}

SecureBlob Mount::GetSystemSalt() {
  return system_salt_;
}

SecureBlob Mount::GetUserSalt(const Credentials& credentials, int index,
                              bool force) {
  FilePath path(GetUserSaltFile(credentials, index));
  return GetOrCreateSalt(path, CRYPTOHOME_DEFAULT_SALT_LENGTH, force);
}

SecureBlob Mount::GetOrCreateSalt(const FilePath& path, int length,
                                  bool force) {
  SecureBlob salt;
  if (force || !file_util::PathExists(path)) {
    // If this salt doesn't exist, automatically create it
    salt.resize(length);
    GetSecureRandom(&salt[0], salt.size());
    int data_written = file_util::WriteFile(path,
        reinterpret_cast<const char*>(&salt[0]),
        length);
    if (data_written != length) {
      LOG(ERROR) << "Could not write user salt";
      return SecureBlob();
    }
  } else {
    // Otherwise just load the contents of the salt
    int64 file_size;
    if (!file_util::GetFileSize(path, &file_size)) {
      LOG(ERROR) << "Could not get size of " << path.value();
      return SecureBlob();
    }
    if (file_size > INT_MAX) {
      LOG(ERROR) << "File " << path.value() << " is too large: " << file_size;
      return SecureBlob();
    }
    salt.resize(file_size);
    int data_read = file_util::ReadFile(path, reinterpret_cast<char*>(&salt[0]),
                                        file_size);
    if (data_read != file_size) {
      LOG(ERROR) << "Could not read entire file " << file_size;
      return SecureBlob();
    }
  }
  return salt;
}

void Mount::AsciiEncodeToBuffer(const chromeos::Blob& blob, char* buffer,
                              int buffer_length) {
  const char hex_chars[] = "0123456789abcdef";
  int i = 0;
  for (chromeos::Blob::const_iterator it = blob.begin();
      it < blob.end() && (i + 1) < buffer_length; ++it) {
    buffer[i++] = hex_chars[((*it) >> 4) & 0x0f];
    buffer[i++] = hex_chars[(*it) & 0x0f];
  }
  if (i < buffer_length) {
    buffer[i] = '\0';
  }
}

bool Mount::AddKeyToEcryptfsKeyring(const VaultKeyset& vault_keyset,
                                    string* key_signature,
                                    string* fnek_signature) {
  // Clear the user keyring
  keyctl(KEYCTL_CLEAR, KEY_SPEC_USER_KEYRING);

  // Add the FEK
  *key_signature = chromeos::AsciiEncode(vault_keyset.FEK_SIG());
  if (!PushVaultKey(vault_keyset.FEK(), *key_signature,
                    vault_keyset.FEK_SALT())) {
    LOG(ERROR) << "Couldn't add ecryptfs key to keyring";
    return false;
  }

  // Add the FNEK
  *fnek_signature = chromeos::AsciiEncode(vault_keyset.FNEK_SIG());
  if (!PushVaultKey(vault_keyset.FNEK(), *fnek_signature,
                    vault_keyset.FNEK_SALT())) {
    LOG(ERROR) << "Couldn't add ecryptfs fnek key to keyring";
    return false;
  }

  return true;
}

bool Mount::PushVaultKey(const SecureBlob& key, const std::string& key_sig,
                         const SecureBlob& salt) {
  DCHECK(key.size() == ECRYPTFS_MAX_KEY_BYTES);
  DCHECK(key_sig.length() == (ECRYPTFS_SIG_SIZE * 2));
  DCHECK(salt.size() == ECRYPTFS_SALT_SIZE);

  struct ecryptfs_auth_tok auth_token;

  generate_payload(&auth_token, const_cast<char*>(key_sig.c_str()),
                   const_cast<char*>(reinterpret_cast<const char*>(&salt[0])),
                   const_cast<char*>(reinterpret_cast<const char*>(&key[0])));

  if (ecryptfs_add_auth_tok_to_keyring(&auth_token,
      const_cast<char*>(key_sig.c_str())) < 0) {
    LOG(ERROR) << "PushVaultKey failed";
  }

  return true;
}

bool Mount::LoadFileBytes(const FilePath& path,
                          SecureBlob& blob) {
  int64 file_size;
  if (!file_util::PathExists(path)) {
    LOG(ERROR) << path.value() << " does not exist!";
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
  blob.swap(buf);
  return true;
}

bool Mount::TerminatePidsWithOpenFiles(const std::string& path, bool hard) {
  std::vector<pid_t> pids = LookForOpenFiles(path);
  for (std::vector<pid_t>::iterator it = pids.begin(); it != pids.end(); it++) {
    pid_t pid = static_cast<pid_t>(*it);
    if (pid != getpid()) {
      if (hard) {
        kill(pid, SIGTERM);
      } else {
        kill(pid, SIGKILL);
      }
    }
  }
  return (pids.size() != 0);
}

std::vector<pid_t> Mount::LookForOpenFiles(const std::string& path_in) {
  // Make sure that if we get a directory, it has a trailing separator
  FilePath file_path(path_in);
  file_util::EnsureEndsWithSeparator(&file_path);
  std::string path = file_path.value();

  std::vector<pid_t> pids;

  // Open /proc
  DIR* proc_dir = opendir(kProcDir.c_str());

  if (!proc_dir) {
    return pids;
  }

  int linkbuf_length = path.length();
  std::vector<char> linkbuf(linkbuf_length);

  // List PIDs in /proc
  while (struct dirent* pid_dirent = readdir(proc_dir)) {
    pid_t pid = static_cast<pid_t>(atoi(pid_dirent->d_name));
    // Ignore PID 1 and errors
    if (pid <= 1) {
      continue;
    }
    // Open /proc/<pid>/fd
    std::string fd_dirname = StringPrintf("%s/%d/fd", kProcDir.c_str(), pid);
    DIR* fd_dir = opendir(fd_dirname.c_str());
    if (!fd_dir) {
      continue;
    }

    // List open file descriptors
    while (struct dirent* fd_dirent = readdir(fd_dir)) {
      std::string fd_path = StringPrintf("%s/%s", fd_dirname.c_str(),
                                         fd_dirent->d_name);
      ssize_t link_length = readlink(fd_path.c_str(), &linkbuf[0],
                                     linkbuf.size());
      if (link_length > 0) {
        std::string open_file(&linkbuf[0], link_length);
        if (open_file.length() >= path.length()) {
          if (open_file.substr(0, path.length()).compare(path) == 0) {
            pids.push_back(pid);
            break;
          }
        }
      }
    }

    closedir(fd_dir);
  }

  closedir(proc_dir);

  return pids;
}

bool Mount::TerminatePidsForUser(const uid_t uid, bool hard) {
  std::vector<pid_t> pids = GetPidsForUser(uid);
  for (std::vector<pid_t>::iterator it = pids.begin(); it != pids.end(); it++) {
    pid_t pid = static_cast<pid_t>(*it);
    if (pid != getpid()) {
      if (hard) {
        kill(pid, SIGTERM);
      } else {
        kill(pid, SIGKILL);
      }
    }
  }
  return (pids.size() != 0);
}

// TODO(fes): Pull this into a separate helper class
std::vector<pid_t> Mount::GetPidsForUser(uid_t uid) {
  std::vector<pid_t> pids;

  // Open /proc
  DIR* proc_dir = opendir(kProcDir.c_str());

  if (!proc_dir) {
    return pids;
  }

  // List PIDs in /proc
  while (struct dirent* pid_dirent = readdir(proc_dir)) {
    pid_t pid = static_cast<pid_t>(atoi(pid_dirent->d_name));
    if (pid <= 0) {
      continue;
    }
    // Open /proc/<pid>/status
    std::string stat_path = StringPrintf("%s/%d/status", kProcDir.c_str(),
                                         pid);
    string contents;
    if (!file_util::ReadFileToString(FilePath(stat_path), &contents)) {
      continue;
    }

    size_t uid_loc = contents.find("Uid:");
    if (!uid_loc) {
      continue;
    }
    uid_loc += 4;

    size_t uid_end = contents.find("\n", uid_loc);
    if (!uid_end) {
      continue;
    }

    contents = contents.substr(uid_loc, uid_end - uid_loc);

    std::vector<std::string> tokens;
    Tokenize(contents, " \t", &tokens);

    for (std::vector<std::string>::iterator it = tokens.begin();
        it != tokens.end(); it++) {
      std::string& value = *it;
      if (value.length() == 0) {
        continue;
      }
      uid_t check_uid = static_cast<uid_t>(atoi(value.c_str()));
      if (check_uid == uid) {
        pids.push_back(pid);
        break;
      }
    }
  }

  closedir(proc_dir);

  return pids;
}

} // cryptohome
