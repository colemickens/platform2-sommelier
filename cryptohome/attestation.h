// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_ATTESTATION_H_
#define CRYPTOHOME_ATTESTATION_H_

#include <base/file_path.h>
#include <base/synchronization/lock.h>
#include <base/threading/platform_thread.h>
#include <chromeos/secure_blob.h>
#include <openssl/evp.h>

#include "attestation.pb.h"

namespace cryptohome {

class Tpm;

// This class performs tasks which enable attestation enrollment.  These tasks
// include creating an AIK and recording all information about the AIK and EK
// that an attestation server will need to issue credentials for this system. If
// a platform does not have a TPM, this class does nothing.
class Attestation : public base::PlatformThread::Delegate {
 public:
  Attestation(Tpm* tpm) : tpm_(tpm),
                          is_prepared_(false),
                          database_path_(kDefaultDatabasePath),
                          thread_(base::kNullThreadHandle) {}
  virtual ~Attestation() {
    if (thread_ != base::kNullThreadHandle)
      base::PlatformThread::Join(thread_);
    ClearDatabase();
  }

  // Returns true if the attestation enrollment blobs already exist.
  virtual bool IsPreparedForEnrollment();

  // Creates attestation enrollment blobs if they do not already exist.
  virtual void PrepareForEnrollment();

  // Like PrepareForEnrollment(), but asynchronous.
  virtual void PrepareForEnrollmentAsync() {
    base::PlatformThread::Create(0, this, &thread_);
  }

  // Verifies all attestation data as an attestation server would. Returns true
  // if all data is valid.
  virtual bool Verify();

  // Sets an alternative attestation database location. Useful in testing.
  virtual void set_database_path(const char* path) {
    database_path_ = FilePath(path);
  }

  // PlatformThread::Delegate interface.
  virtual void ThreadMain() { PrepareForEnrollment(); }

 private:
  enum FirmwareType {
    kUnknown,
    kVerified,
    kDeveloper
  };
  static const size_t kQuoteExternalDataSize;
  static const size_t kCipherKeySize;
  static const size_t kCipherBlockSize;
  static const size_t kNonceSize;
  static const char* kDefaultDatabasePath;
  static const struct CertificateAuthority {
    const char* issuer;
    const char* modulus;  // In hex format.
  } kKnownEndorsementCA[];
  static const struct PCRValue {
    bool developer_mode_enabled;
    bool recovery_mode_enabled;
    FirmwareType firmware_type;
  } kKnownPCRValues[];

  Tpm* tpm_;
  bool is_prepared_;
  base::Lock prepare_lock_;
  chromeos::SecureBlob database_key_;
  FilePath database_path_;
  AttestationDatabase database_pb_;
  base::PlatformThreadHandle thread_;

  // Moves data from a std::string container to a SecureBlob container.
  chromeos::SecureBlob ConvertStringToBlob(const std::string& s);

  // Moves data from a chromeos::Blob container to a std::string container.
  std::string ConvertBlobToString(const chromeos::Blob& blob);

  // Serializes and encrypts an attestation database.
  bool EncryptDatabase(const AttestationDatabase& db,
                       EncryptedDatabase* encrypted_db);

  // Decrypts and parses an attestation database.
  bool DecryptDatabase(const EncryptedDatabase& encrypted_db,
                       AttestationDatabase* db);

  // Computes an encrypted database HMAC.
  std::string ComputeHMAC(const EncryptedDatabase& encrypted_db);

  // Writes an encrypted database to a persistent storage location.
  bool StoreDatabase(const EncryptedDatabase& encrypted_db);

  // Reads a database from a persistent storage location.
  bool LoadDatabase(EncryptedDatabase* encrypted_db);

  // Verifies an endorsement credential against known Chrome OS issuers.
  bool VerifyEndorsementCredential(const chromeos::SecureBlob& credential,
                                   const chromeos::SecureBlob& public_key);

  // Verifies identity key binding data.
  bool VerifyIdentityBinding(const IdentityBinding& binding);

  // Verifies a quote of PCR0.
  bool VerifyQuote(const chromeos::SecureBlob& aik_public_key,
                   const Quote& quote);

  // Verifies a certified key.
  bool VerifyCertifiedKey(const chromeos::SecureBlob& aik_public_key,
                          const chromeos::SecureBlob& certified_public_key,
                          const chromeos::SecureBlob& certified_key_info,
                          const chromeos::SecureBlob& proof);

  // Creates a public key based on a known credential issuer.
  EVP_PKEY* GetAuthorityPublicKey(const char* issuer_name);

  // Verifies an RSA-PKCS1-SHA1 digital signature.
  bool VerifySignature(const chromeos::SecureBlob& public_key,
                       const chromeos::SecureBlob& signed_data,
                       const chromeos::SecureBlob& signature);

  // Clears the memory of the database protobuf.
  void ClearDatabase();

  // Clears the memory of a std::string.
  void ClearString(std::string* s);

  DISALLOW_COPY_AND_ASSIGN(Attestation);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME__ATTESTATION_H_
