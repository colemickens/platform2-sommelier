// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_
#define ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_

#include <string>

#include "attestation/common/attestation_ca.pb.h"
#include "attestation/common/export.h"
#include "attestation/common/interface.pb.h"

namespace attestation {

// The main attestation interface implemented by proxies and services. The
// anticipated flow looks like this:
//   [APP] -> AttestationInterface -> [IPC] -> AttestationInterface
class ATTESTATION_EXPORT AttestationInterface {
 public:
  virtual ~AttestationInterface() {}

  // Performs initialization tasks that may take a long time. This method must
  // be successfully called before calling any other method. Returns true on
  // success.
  virtual bool Initialize() = 0;

  // Creates a key certified by the Google Attestation CA which corresponds to
  // the give |key_label|, |key_type|, and |key_usage|. The certificate issued
  // by the CA will correspond to |certificate_profile|. On success,
  // |certificate| will contain the DER-encoded X.509 certificate issued by the
  // CA. If the CA refuses to issue a certificate, REQUEST_DENIED_BY_CA is
  // returned and |server_error_details| contains a message from the CA.
  virtual AttestationStatus CreateGoogleAttestedKey(
      const std::string& key_label,
      KeyType key_type,
      KeyUsage key_usage,
      CertificateProfile certificate_profile,
      std::string* certificate,
      std::string* server_error_details) = 0;
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_
