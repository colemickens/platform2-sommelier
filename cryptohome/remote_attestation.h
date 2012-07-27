// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_REMOTE_ATTESTATION_H_
#define CRYPTOHOME_REMOTE_ATTESTATION_H_

#include <base/file_path.h>
#include <base/synchronization/lock.h>
#include <chromeos/secure_blob.h>

#include "remote_attestation.pb.h"

namespace cryptohome {

class Tpm;

// This class performs tasks which enable remote attestation.  These tasks
// include creating an AIK and recording all information about the AIK and EK
// that an attestation server will need to issue credentials for this system. If
// a platform does not have a TPM, this class does nothing.
class RemoteAttestation {
 public:
  RemoteAttestation(Tpm* tpm) : tpm_(tpm),
                                is_prepared_(false),
                                database_path_(kDefaultDatabasePath) {}
  virtual ~RemoteAttestation() {}

  // Returns true if the remote attestation enrollment blobs already exist.
  virtual bool IsPreparedForEnrollment();

  // Creates remote attestation enrollment blobs if they do not already exist.
  virtual void PrepareForEnrollment();

  // Like PrepareForEnrollment(), but asynchronous.
  virtual void PrepareForEnrollmentAsync();

  // Sets an alternative attestation database location. Useful in testing.
  virtual void set_database_path(const char* path) {
    database_path_ = FilePath(path);
  }

 private:
  static const size_t kQuoteExternalDataSize;
  static const size_t kCipherKeySize;
  static const size_t kCipherBlockSize;
  static const char* kDefaultDatabasePath;

  Tpm* tpm_;
  bool is_prepared_;
  base::Lock prepare_lock_;
  chromeos::SecureBlob database_key_;
  FilePath database_path_;

  // Moves data from a std::string container to a SecureBlob container.
  chromeos::SecureBlob ConvertStringToBlob(const std::string& s);

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

  DISALLOW_COPY_AND_ASSIGN(RemoteAttestation);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_REMOTE_ATTESTATION_H_
