// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mount - class for managing cryptohome user keys and mounts.  In Chrome OS,
// users are managed on top of a shared unix user, chronos.  When a user logs
// in, cryptohome mounts their encrypted home directory to /home/chronos/user,
// and Chrome does a profile switch to that directory.  All user data in their
// home directory is transparently encrypted, providing protection against
// offline theft.  On logout, the mount point is removed.

#ifndef CRYPTOHOME_MOUNT_H_
#define CRYPTOHOME_MOUNT_H_

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/scoped_ptr.h>

#include "credentials.h"
#include "crypto.h"
#include "platform.h"
#include "secure_blob.h"
#include "user_session.h"
#include "vault_keyset.h"

namespace cryptohome {

// The directory to mount the user's cryptohome at
extern const std::string kDefaultHomeDir;
// The directory containing the system salt and the user vaults
extern const std::string kDefaultShadowRoot;
// The default shared user (chronos)
extern const std::string kDefaultSharedUser;
// The default skeleton source (/etc/skel)
extern const std::string kDefaultSkeletonSource;
// The incognito user
extern const std::string kIncognitoUser;

// The Mount class handles mounting/unmounting of the user's cryptohome
// directory as well as offline verification of the user's credentials against
// the directory's crypto key.
class Mount : public EntropySource {
 public:
  enum MountError {
    MOUNT_ERROR_NONE = 0,
    MOUNT_ERROR_FATAL = 1 << 0,
    MOUNT_ERROR_KEY_FAILURE = 1 << 1,
    MOUNT_ERROR_MOUNT_POINT_BUSY = 1 << 2,
    MOUNT_ERROR_TPM_COMM_ERROR = 1 << 3,
    MOUNT_ERROR_TPM_DEFEND_LOCK = 1 << 4,
    MOUNT_ERROR_RECREATED = 1 << 31,
  };

  // Sets up Mount with the default locations, username, etc., as defined above.
  Mount();

  virtual ~Mount();

  // Gets the uid/gid of the default user and loads the system salt
  virtual bool Init();

  // Attempts to mount the cryptohome for the given credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   error - The specific error condition on failure
  virtual bool MountCryptohome(const Credentials& credentials,
                               MountError* error) const;

  // Unmounts any mount at the cryptohome mount point
  virtual bool UnmountCryptohome() const;

  // Remove a user's cryptohome
  virtual bool RemoveCryptohome(const Credentials& credentials) const;

  // Checks if the mount point currently has a mount
  virtual bool IsCryptohomeMounted() const;

  // Checks if the mount point currently has a mount, and if that mount is for
  // the specified credentials
  //
  // Parameters
  //   credentials - The Credentials for which to test the mount point
  virtual bool IsCryptohomeMountedForUser(const Credentials& credentials) const;

  // Checks if the cryptohome vault exists for the given credentials and creates
  // it if not (calls CreateCryptohome).
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be ensured.
  //   created (OUT) - Whether the cryptohome was created
  virtual bool EnsureCryptohome(const Credentials& credentials,
                                bool* created) const;

  // Creates the cryptohome salt, key, and vault for the specified credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be created.
  virtual bool CreateCryptohome(const Credentials& credentials) const;

  // Tests if the given credentials would unwrap the user's cryptohome key
  //
  // Parameters
  //   credentials - The Credentials to attempt to unwrap the key with
  virtual bool TestCredentials(const Credentials& credentials) const;

  // Migrages a user's vault key from one passkey to another
  //
  // Parameters
  //   credentials - The new Credentials for the user
  //   from_key - The old Credentials
  virtual bool MigratePasskey(const Credentials& credentials,
                              const char* old_key) const;

  // Mounts a guest home directory to the cryptohome mount point
  virtual bool MountGuestCryptohome() const;

  // Returns the system salt
  virtual void GetSystemSalt(chromeos::Blob* salt) const;

  // Used to disable setting vault ownership
  void set_set_vault_ownership(bool value) {
    set_vault_ownership_ = value;
  }

  // Used to override the default home directory
  void set_home_dir(const std::string& value) {
    home_dir_ = value;
  }

  // Used to override the default shadow root
  void set_shadow_root(const std::string& value) {
    shadow_root_ = value;
  }

  // Get the shadow root
  std::string get_shadow_root() {
    return shadow_root_;
  }

  // Used to override the default shared username
  void set_shared_user(const std::string& value) {
    default_username_ = value;
  }

  // Used to override the default skeleton directory
  void set_skel_source(const std::string& value) {
    skel_source_ = value;
  }

  // Used to override the default Crypto handler (does not take ownership)
  void set_crypto(Crypto* value) {
    crypto_ = value;
  }

  // Get the Crypto instance
  Crypto* get_crypto() {
    return crypto_;
  }

  // Used to override the default Platform handler (does not take ownership)
  void set_platform(Platform* value) {
    platform_ = value;
  }

  // Override whether to use scrypt for added security when the TPM is off or
  // disabled
  void set_fallback_to_scrypt(bool value) {
    fallback_to_scrypt_ = value;
  }

  // Override whether to use the TPM for added security
  void set_use_tpm(bool value) {
    use_tpm_ = value;
  }

  // Manually set the logged in user
  void set_current_user(UserSession* value) {
    current_user_ = value;
  }

  // Get the user session
  UserSession* get_current_user() {
    return current_user_;
  }

  // Manually set the logged in user
  void set_current_user_credentials(const Credentials& credentials) {
    current_user_->SetUser(credentials);
  }

  // Manually clear the current logged-in user
  void reset_current_user_credentials() {
    current_user_->Reset();
  }

  // Loads the contents of the specified file as a blob
  //
  // Parameters
  //   path - The file path to read from
  //   blob (OUT) - Where to store the loaded file bytes
  static bool LoadFileBytes(const FilePath& path, SecureBlob* blob);

  // Loads the contents of the specified file as a string
  //
  // Parameters
  //   path - The file path to read from
  //   content (OUT) - The string value
  static bool LoadFileString(const FilePath& path, std::string* content);

  // Saves the VaultKeyset for the user
  //
  // Parameters
  //   credentials - The Credentials for the user
  //   vault_keyset - The VaultKeyset to save
  bool SaveVaultKeyset(const Credentials& credentials,
                       const VaultKeyset& vault_keyset) const;

  // Resaves the vault keyset, restoring on failure
  //
  // Parameters
  //   credentials - The Credentials for the user
  //   vault_keyset - The VaultKeyset to save
  bool ResaveVaultKeyset(const Credentials& credentials,
                         const VaultKeyset& vault_keyset) const;

  // Attempt to unwrap the keyset for the specified user
  //
  // Parameters
  //   credentials - The user credentials to use
  //   vault_keyset (OUT) - The unencrypted vault keyset on success
  //   error (OUT) - The specific error when unwrapping
  bool UnwrapVaultKeyset(const Credentials& credentials, bool migrate_if_needed,
                         VaultKeyset* vault_keyset,
                         MountError* error) const;

  // Saves the VaultKeyset for the user in the old method
  //
  // Parameters
  //   credentials - The Credentials for the user
  //   vault_keyset - The VaultKeyset to save
  bool SaveVaultKeysetOld(const Credentials& credentials,
                          const VaultKeyset& vault_keyset) const;

  // Attempt to unwrap the keyset using the old method for a user
  //
  // Parameters
  //   credentials - The user credentials to use
  //   vault_keyset (OUT) - The unencrypted vault keyset on success
  //   error (OUT) - The specific error when unwrapping
  bool UnwrapVaultKeysetOld(const Credentials& credentials,
                            VaultKeyset* vault_keyset, MountError* error) const;

  // Remove the key file and (old) salt file if they exist
  //
  // Parameters
  //   credentials - The user credentials to remove the files for
  bool RemoveOldFiles(const Credentials& credentials) const;

  // Cache the old key file and salt file during migration
  //
  // Parameters
  //   credentials - The user credentials to cache the files for
  //   files - The file names to cache
  bool CacheOldFiles(const Credentials& credentials,
                     std::vector<std::string>& files) const;

  // Move the cached files back to the original files
  //
  // Parameters
  //   credentials - The user credentials to un-cache the files for
  //   files - The file names to un-cache
  bool RevertCacheFiles(const Credentials& credentials,
                        std::vector<std::string>& files) const;

  // Remove the cached files for the user
  //
  // Parameters
  //   credentials - The user credentials to remove the files for
  //   files - The file names to remove
  bool DeleteCacheFiles(const Credentials& credentials,
                        std::vector<std::string>& files) const;

  // Gets the directory in the shadow root where the user's salt, key, and vault
  // are stored.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserDirectory(const Credentials& credentials) const;

  // Gets the user's key file name
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserKeyFile(const Credentials& credentials) const;

  // Gets the user's salt file name
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserSaltFile(const Credentials& credentials) const;

  // Gets the user's vault directory
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserVaultPath(const Credentials& credentials) const;

 private:
  // Same as MountCryptohome but specifies if the cryptohome directory should be
  // recreated on a fatal error
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   recreate_unwrap_fatal - Attempt to recreate the cryptohome directory on a
  //                           fatal error (for example, TPM was cleared)
  //   error - The specific error condition on failure
  virtual bool MountCryptohomeInner(const Credentials& credentials,
                                    bool recreate_unwrap_fatal,
                                    MountError* error) const;

  // Recursively copies directory contents to the destination if the destination
  // file does not exist.  Sets ownership to the default_user_
  //
  // Parameters
  //   destination - Where to copy files to
  //   source - Where to copy files from
  void RecursiveCopy(const FilePath& destination, const FilePath& source) const;

  // Copies the skeleton directory to the user's cryptohome if that user is
  // currently mounted
  //
  // Parameters
  //   credentials - The Credentials representing the user
  void CopySkeletonForUser(const Credentials& credentials) const;

  // Copies the skeleton directory to the cryptohome mount point
  //
  void CopySkeleton() const;

  // Returns the specified number of random bytes
  //
  // Parameters
  //   rand (IN/OUT) - Where to store the bytes, must be a least length bytes
  //   length - The number of random bytes to return
  void GetSecureRandom(unsigned char *rand, int length) const;

  // Creates a new master key and stores it in the master key file for a user
  //
  // Parameters
  //   credentials - The Credentials representing the user
  bool CreateMasterKey(const Credentials& credentials) const;

  // Returns the user's salt
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   force - Whether to force creation of a new salt
  //   salt (OUT) - The user's salt
  void GetUserSalt(const Credentials& credentials, bool force_new,
                   SecureBlob* salt) const;

  // The uid of the shared user.  Ownership of the user's vault is set to this
  // uid.
  uid_t default_user_;

  // The gid of the shared user.  Ownership of the user's vault is set to this
  // gid.
  gid_t default_group_;

  // The shared user name.  This user's uid/gid is used for vault ownership.
  std::string default_username_;

  // The file path to mount cryptohome at.  Defaults to /home/chronos/user
  std::string home_dir_;

  // Where to store the system salt and user salt/key/vault.  Defaults to
  // /home/chronos/shadow
  std::string shadow_root_;

  // Where the skeleton for the user's cryptohome is copied from
  std::string skel_source_;

  // Stores the global system salt
  cryptohome::SecureBlob system_salt_;

  // Whether to change ownership of the vault file
  bool set_vault_ownership_;

  // The crypto implementation
  scoped_ptr<Crypto> default_crypto_;
  Crypto *crypto_;

  // The platform-specific calls
  scoped_ptr<Platform> default_platform_;
  Platform *platform_;

  // Whether to use scrypt for added security when use of the TPM is disabled
  bool fallback_to_scrypt_;

  // Whether to use the TPM for added security
  bool use_tpm_;

  // Used to keep track of the current logged-in user
  scoped_ptr<UserSession> default_current_user_;
  UserSession* current_user_;

  DISALLOW_COPY_AND_ASSIGN(Mount);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOUNT_H_
