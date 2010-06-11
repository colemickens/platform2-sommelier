// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mount - class for managing cryptohome user keys and mounts.  In Chrome OS,
// users are managed on top of a shared unix user, chronos.  When a user logs
// in, cryptohome mounts their encrypted home directory to /home/chronos/user,
// and Chrome does a profile switch to that directory.  All user data in their
// home directory is transparently encrypted, providing protection against
// offline theft.  On logout, the mount point is removed.
//
// Cryptohome manages directories as follows:
//
// /home/.shadow                            : Location for the system salt and
//                                            individual users' salt/key/vault
// /home/.shadow/<salted_hash_of_username>  : Each Chrome OS user gets a
//                                            directory in the shadow root where
//                                            their salts, keys, and vault are
//                                            stored.
// /home/.shadow/<s_h_o_u>/vault            : The user's vault (the encrypted
//                                            version of their home directory)
// /home/.shadow/<s_h_o_u>/master.X         : Vault key at index X.  Key indexes
//                                            may be used during password
//                                            change.  The vault key is
//                                            encrypted using a key generated
//                                            from the user's passkey and the
//                                            key-specific salt.
// /home/.shadow/<s_h_o_u>/master.X.salt    : Salt used in converting the user's
//                                            passkey to a passkey wrapper
//                                            (what encrypts the vault key) at
//                                            index X.
// /home/chronos/user                       : On successful login, the user's
//                                            vault directory is mounted here
//                                            using the symmetric key decrypted
//                                            from master.X by the user's
//                                            passkey.
//
// Offline login and screen unlock is processed through cryptohome using a test
// decryption of any of the user's master keys using the passkey provided.
//
// A user's cryptohome is automatically created when the vault directory for the
// user does not exist and the cryptohome service gets a call to mount the
// user's home directory.
//
// Passkey change: <TBD>

#ifndef CRYPTOHOME_MOUNT_H_
#define CRYPTOHOME_MOUNT_H_

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/scoped_ptr.h>

#include "credentials.h"
#include "crypto.h"
#include "platform.h"
#include "secure_blob.h"
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
    MOUNT_ERROR_NO_SUCH_FILE = 1 << 3,
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
  //   index - The key index to try
  //   error - The specific error condition on failure
  virtual bool MountCryptohome(const Credentials& credentials, int index,
                               MountError* error = NULL);

  // Unmounts any mount at the cryptohome mount point
  virtual bool UnmountCryptohome();

  // Remove a user's cryptohome
  virtual bool RemoveCryptohome(const Credentials& credentials);

  // Checks if the mount point currently has a mount
  virtual bool IsCryptohomeMounted();

  // Checks if the mount point currently has a mount, and if that mount is for
  // the specified credentials
  //
  // Parameters
  //   credentials - The Credentials for which to test the mount point
  virtual bool IsCryptohomeMountedForUser(const Credentials& credentials);

  // Creates the cryptohome salt, key, and vault for the specified credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be created.
  //   index - The key index to generate
  virtual bool CreateCryptohome(const Credentials& credentials, int index);

  // Tests if the given credentials would unwrap the user's cryptohome key
  //
  // Parameters
  //   credentials - The Credentials to attempt to unwrap the key with
  virtual bool TestCredentials(const Credentials& credentials);

  // Migrages a user's vault key from one passkey to another
  //
  // Parameters
  //   credentials - The new Credentials for the user
  //   from_key - The old Credentials
  virtual bool MigratePasskey(const Credentials& credentials,
                              const char* old_key);

  // Mounts a guest home directory to the cryptohome mount point
  virtual bool MountGuestCryptohome();

  // Returns the system salt
  virtual void GetSystemSalt(chromeos::Blob* salt);

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

  // Used to override the default Platform handler (does not take ownership)
  void set_platform(Platform* value) {
    platform_ = value;
  }

 private:
  // Checks if the cryptohome vault exists for the given credentials and creates
  // it if not (calls CreateCryptohome).
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be ensured.
  //   created (OUT) - Whether the cryptohome was created
  virtual bool EnsureCryptohome(const Credentials& credentials,
                                bool* created);

  // Saves the VaultKeyset for the user at the given index
  //
  // Parameters
  //   credentials - The Credentials for the user
  //   vault_keyset - The VaultKeyset to save
  //   index - The index to save the VaultKeyset to
  bool SaveVaultKeyset(const Credentials& credentials,
                       const VaultKeyset& vault_keyset,
                       int index);

  // Gets the directory in the shadow root where the user's salt, key, and vault
  // are stored.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserDirectory(const Credentials& credentials);

  // Gets the user's key file name at the given index
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   index - The key index to return
  std::string GetUserKeyFile(const Credentials& credentials, int index);

  // Gets the user's salt file name at the given index
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   index - The salt index to return
  std::string GetUserSaltFile(const Credentials& credentials, int index);

  // Gets the user's vault directory
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserVaultPath(const Credentials& credentials);

  // Recursively copies directory contents to the destination if the destination
  // file does not exist.  Sets ownership to the default_user_
  //
  // Parameters
  //   destination - Where to copy files to
  //   source - Where to copy files from
  void RecursiveCopy(const FilePath& destination, const FilePath& source);

  // Copies the skeleton directory to the user's cryptohome if that user is
  // currently mounted
  //
  // Parameters
  //   credentials - The Credentials representing the user
  void CopySkeletonForUser(const Credentials& credentials);

  // Copies the skeleton directory to the cryptohome mount point
  //
  void CopySkeleton();

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
  //   index - the key index to generate
  bool CreateMasterKey(const Credentials& credentials, int index);

  // Returns the user's salt at the given index
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   index - The salt index to return
  //   force - Whether to force creation of a new salt
  //   salt (OUT) - The user's salt
  void GetUserSalt(const Credentials& credentials, int index,
                   bool force_new, SecureBlob* salt);

  // Loads the contents of the specified file as a blob
  //
  // Parameters
  //   path - The file path to read from
  //   blob (OUT) - Where to store the loaded file bytes
  bool LoadFileBytes(const FilePath& path, SecureBlob* blob);

  // Attempt to unwrap the keyset for the specified user
  //
  // Parameters
  //   credentials - The user credentials to use
  //   index - The keyset index to unwrap
  //   vault_keyset (OUT) - The unencrypted vault keyset on success
  //   error (OUT) - The specific error when unwrapping
  bool UnwrapVaultKeyset(const Credentials& credentials, int index,
                         VaultKeyset* vault_keyset, MountError* error);

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

 private:
  DISALLOW_COPY_AND_ASSIGN(Mount);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOUNT_H_
