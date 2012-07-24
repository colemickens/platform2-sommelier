// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Crypto - class for handling the keyset key management functions relating to
// cryptohome.  This includes wrapping/unwrapping the vault keyset (and
// supporting functions) and setting/clearing the user keyring for use with
// ecryptfs.

#ifndef CRYPTOHOME_CRYPTO_H_
#define CRYPTOHOME_CRYPTO_H_

#include <base/basictypes.h>
#include <base/file_path.h>
#include <chromeos/secure_blob.h>

#include "tpm.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

namespace cryptohome {

// Default number of hash rounds to use when generating  key from a password
extern const unsigned int kDefaultPasswordRounds;

class Crypto {
 public:
  // PaddingScheme dictates the padding at the end of the ciphertext:
  //   kPaddingNone - Do not use padding.  The input plaintext must be a
  //     multiple of the crypto algorithm's block size.  This is used in the
  //     encryption of the vault keyset as described in the Protection
  //     Mechanisms section of the README file.
  //   kPaddingCryptohomeDefault - The default padding, which adds a SHA1 hash
  //     to the end of the plaintext before encryption so that the contents can
  //     be verified.
  enum PaddingScheme {
    kPaddingNone = 0,
    kPaddingCryptohomeDefault = 2,
  };

  enum BlockMode {
    kEcb = 1,
    kCbc = 2,
  };

  enum CryptoError {
    CE_NONE = 0,
    CE_TPM_FATAL,
    CE_TPM_COMM_ERROR,
    CE_TPM_DEFEND_LOCK,
    CE_TPM_CRYPTO,
    CE_SCRYPT_CRYPTO,
    CE_OTHER_FATAL,
    CE_OTHER_CRYPTO,
    CE_NO_PUBLIC_KEY_HASH,
  };

  // Default constructor
  Crypto();

  virtual ~Crypto();

  // Initializes Crypto
  bool Init(Platform* platform);

  // Returns random bytes of the given length using OpenSSL's random number
  // generator.  The random number generator is automatically seeded from
  // /dev/urandom by OpenSSL.
  //
  // Parameters
  //   rand (OUT) - Where to store the random bytes
  //   length - The number of random bytes to store in rand
  void GetSecureRandom(unsigned char* rand, unsigned int length) const;

  // Gets the AES block size
  unsigned int GetAesBlockSize() const;

  // AES decrypts the wrapped blob
  //
  // Parameters
  //   wrapped - The blob containing the encrypted data
  //   key - The AES key to use in decryption
  //   iv - The initialization vector to use
  //   plaintext - The unwrapped (decrypted) data
  bool AesDecrypt(const chromeos::Blob& ciphertext,
                  const chromeos::SecureBlob& key,
                  const chromeos::SecureBlob& iv,
                  chromeos::SecureBlob* plaintext) const;

  // AES encrypts the plain text data using the specified key
  //
  // Parameters
  //   plaintext - The plain text data to encrypt
  //   key - The AES key to use
  //   iv - The initialization vector to use
  //   ciphertext - On success, the encrypted data
  bool AesEncrypt(const chromeos::Blob& plaintext,
                  const chromeos::SecureBlob& key,
                  const chromeos::SecureBlob& iv,
                  chromeos::SecureBlob* ciphertext) const;

  // Same as AesDecrypt, but allows using either CBC or ECB
  bool AesDecryptSpecifyBlockMode(const chromeos::Blob& ciphertext,
                                  unsigned int start, unsigned int count,
                                  const chromeos::SecureBlob& key,
                                  const chromeos::SecureBlob& iv,
                                  PaddingScheme padding, BlockMode block_mode,
                                  chromeos::SecureBlob* plaintext) const;

  // Same as AesEncrypt, but allows using either CBC or ECB
  bool AesEncryptSpecifyBlockMode(const chromeos::Blob& plaintext,
                                  unsigned int start, unsigned int count,
                                  const chromeos::SecureBlob& key,
                                  const chromeos::SecureBlob& iv,
                                  PaddingScheme padding, BlockMode block_mode,
                                  chromeos::SecureBlob* ciphertext) const;

  // Creates a new RSA key
  //
  // Parameters
  //   key_bits - The key size to generate
  //   n (OUT) - the modulus
  //   p (OUT) - the private key
  bool CreateRsaKey(unsigned int key_bits,
                    chromeos::SecureBlob* n,
                    chromeos::SecureBlob* p) const;

  // Decrypts an encrypted vault keyset.  The vault keyset should be the output
  // of EncryptVaultKeyset().
  //
  // Parameters
  //   encrypted_keyset - The blob containing the encrypted keyset
  //   vault_key - The passkey used to decrypt the keyset
  //   crypt_flags (OUT) - Whether the keyset was wrapped by the TPM or scrypt
  //   error (OUT) - The specific error code on failure
  //   vault_keyset (OUT) - The decrypted vault keyset on success
  bool DecryptVaultKeyset(const SerializedVaultKeyset& serialized,
                          const chromeos::SecureBlob& vault_key,
                          unsigned int* crypt_flags, CryptoError* error,
                          VaultKeyset* vault_keyset) const;

  // Encrypts the vault keyset with the given passkey
  //
  // Parameters
  //   vault_keyset - The VaultKeyset to encrypt
  //   vault_key - The passkey used to encrypt the keyset
  //   vault_key_salt - The salt to use for the vault passkey to key conversion
  //                    when encrypting the keyset
  //   encrypted_keyset - On success, the encrypted vault keyset
  bool EncryptVaultKeyset(const VaultKeyset& vault_keyset,
                          const chromeos::SecureBlob& vault_key,
                          const chromeos::SecureBlob& vault_key_salt,
                          SerializedVaultKeyset* serialized) const;

  // Converts the passkey directly to an AES key.  This method derives the key
  // using the default OpenSSL conversion method.
  //
  // Parameters
  //   passkey - The passkey (hash, currently) to create the key from
  //   salt - The salt used in creating the key
  //   rounds - The number of SHA rounds to perform
  //   key (OUT) - The AES key
  //   iv (OUT) - The initialization vector
  bool PasskeyToAesKey(const chromeos::Blob& passkey,
                       const chromeos::Blob& salt, unsigned int rounds,
                       chromeos::SecureBlob* key,
                       chromeos::SecureBlob* iv) const;

  // Converts the passkey to authorization data for a TPM-backed crypto token.
  //
  // Parameters
  //   passkey - The passkey from which to derive the authorization data.
  //   salt - The salt file used in deriving the authorization data.
  //   auth_data (OUT) - The token authorization data.
  bool PasskeyToTokenAuthData(const chromeos::Blob& passkey,
                              const FilePath& salt_file,
                              chromeos::SecureBlob* auth_data) const;

  // Gets an existing salt, or creates one if it doesn't exist
  //
  // Parameters
  //   path - The path to the salt file
  //   length - The length of the new salt if it needs to be created
  //   force - If true, forces creation of a new salt even if the file exists
  //   salt (OUT) - The salt
  bool GetOrCreateSalt(const FilePath& path, unsigned int length, bool force,
                       chromeos::SecureBlob* salt) const;

  // Adds the specified key to the ecryptfs keyring so that the cryptohome can
  // be mounted.  Clears the user keyring first.
  //
  // Parameters
  //   vault_keyset - The keyset to add
  //   key_signature (OUT) - The signature of the cryptohome key that should be
  //     used in subsequent calls to mount(2)
  //   filename_key_signature (OUT) - The signature of the cryptohome filename
  //     encryption key that should be used in subsequent calls to mount(2)
  bool AddKeyset(const VaultKeyset& vault_keyset,
                 std::string* key_signature,
                 std::string* filename_key_signature) const;

  // Clears the user's kernel keyring
  void ClearKeyset() const;

  // Gets the SHA1 hash of the data provided
  chromeos::SecureBlob GetSha1(const chromeos::Blob& data) const;

  // Gets the SHA256 hash of the data provided
  chromeos::SecureBlob GetSha256(const chromeos::Blob& data) const;

  // Encodes a binary blob to hex-ascii
  //
  // Parameters
  //   blob - The binary blob to convert
  //   buffer (IN/OUT) - Where to store the converted blob
  //   buffer_length - The size of the buffer
  static void AsciiEncodeToBuffer(const chromeos::Blob& blob, char* buffer,
                                  unsigned int buffer_length);

  // Converts a null-terminated password to a passkey (ascii-encoded first half
  // of the salted SHA1 hash of the password).
  //
  // Parameters
  //   password - The password to convert
  //   salt - The salt used during hashing
  //   passkey (OUT) - The passkey
  static void PasswordToPasskey(const char* password,
                                const chromeos::Blob& salt,
                                chromeos::SecureBlob* passkey);

  // Ensures that the TPM is connected
  CryptoError EnsureTpm(bool disconnect_first) const;

  // Sets whether or not to use the TPM (must be called before init, depends
  // on the presence of a functioning, initialized TPM).  The TPM is merely used
  // to add a layer of difficulty in a brute-force attack against the user's
  // credentials.
  void set_use_tpm(bool value) {
    use_tpm_ = value;
  }

  // Sets the TPM implementation
  void set_tpm(Tpm* value) {
    tpm_ = value;
  }

  // Gets whether the TPM is set
  bool has_tpm() {
    return (tpm_ != NULL);
  }

  // Gets the TPM implementation
  const Tpm* get_tpm() {
    return tpm_;
  }

  // Checks if the TPM is connected
  bool is_tpm_connected() {
    if (tpm_ == NULL) {
      return false;
    }
    return tpm_->IsConnected();
  }

  static const int64 kSaltMax;

 private:
  // Converts a TPM error to a Crypto error
  CryptoError TpmErrorToCrypto(Tpm::TpmRetryAction retry_action) const;

  // Adds the specified key to the user keyring
  //
  // Parameters
  //   key - The key to add
  //   key_sig - The key's (ascii) signature
  //   salt - The salt
  bool PushVaultKey(const chromeos::SecureBlob& key,
                    const std::string& key_sig,
                    const chromeos::SecureBlob& salt) const;

  bool EncryptTPM(const chromeos::SecureBlob& blob,
                  const chromeos::SecureBlob& key,
                  const chromeos::SecureBlob& salt,
                  SerializedVaultKeyset* serialized) const;

  bool EncryptScrypt(const chromeos::SecureBlob& blob,
                     const chromeos::SecureBlob& key,
                     SerializedVaultKeyset* serialized) const;

  bool DecryptTPM(const SerializedVaultKeyset& serialized,
                  const chromeos::SecureBlob& key,
                  CryptoError* error,
                  VaultKeyset* vault_keyset) const;

  bool DecryptScrypt(const SerializedVaultKeyset& serialized,
                     const chromeos::SecureBlob& key,
                     CryptoError* error,
                     VaultKeyset* keyset) const;

  bool IsTPMPubkeyHash(const std::string& hash, CryptoError* error) const;

  // If set, the TPM will be used during the encryption of the vault keyset
  bool use_tpm_;

  // The TPM implementation
  Tpm* tpm_;

  DISALLOW_COPY_AND_ASSIGN(Crypto);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CRYPTO_H_
