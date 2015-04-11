// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_SERVER_ATTESTATION_SERVICE_H_
#define ATTESTATION_SERVER_ATTESTATION_SERVICE_H_

#include "attestation/common/attestation_interface.h"

#include <memory>
#include <string>

#include <base/macros.h>

#include "attestation/server/crypto_utility.h"
#include "attestation/server/database.h"

namespace attestation {

// An implementation of AttestationInterface for the core attestation service.
// Usage:
//   std::unique_ptr<AttestationInterface> attestation =
//       new AttestationService();
//   CHECK(attestation->Initialize());
//   attestation->CreateGoogleAttestedKey(...);
class AttestationService : public AttestationInterface {
 public:
  AttestationService() = default;
  ~AttestationService() override = default;

  // AttestationInterface methods.
  bool Initialize() override;
  void CreateGoogleAttestedKey(
      const std::string& key_label,
      KeyType key_type,
      KeyUsage key_usage,
      CertificateProfile certificate_profile,
      const base::Callback<CreateGoogleAttestedKeyCallback>& callback) override;

  // Useful for testing.
  void set_database(Database* database) {
    database_ = database;
  }

 private:
  CryptoUtility* crypto_;
  Database* database_;
  std::unique_ptr<Database> default_database_;

  DISALLOW_COPY_AND_ASSIGN(AttestationService);
};

}  // namespace attestation

#endif  // ATTESTATION_SERVER_ATTESTATION_SERVICE_H_
