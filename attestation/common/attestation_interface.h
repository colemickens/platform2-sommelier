// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_
#define ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_

#include <string>

#include <base/callback_forward.h>

#include "attestation/common/export.h"
#include "attestation/common/interface.pb.h"

namespace attestation {

// The main attestation interface implemented by proxies and services. The
// anticipated flow looks like this:
//   [APP] -> AttestationInterface -> [IPC] -> AttestationInterface
class ATTESTATION_EXPORT AttestationInterface {
 public:
  virtual ~AttestationInterface() = default;

  // Performs initialization tasks that may take a long time. This method must
  // be successfully called before calling any other method. Returns true on
  // success.
  virtual bool Initialize() = 0;

  // Creates a key certified by the Google Attestation CA which corresponds to
  // the given |key_label|, |key_type|, and |key_usage|. The certificate issued
  // by the CA will correspond to |certificate_profile|. On success, |status|
  // will be SUCCESS and |certificate_chain| will contain a PEM-encoded list of
  // X.509 certificates starting with the requested certificate issued by the CA
  // and followed by certificates for any intermediate authorities, in order.
  // The Google Attestation CA root certificate is well-known and not included.
  // If the CA refuses to issue a certificate, |status| will be
  // REQUEST_DENIED_BY_CA and |server_error_details| will contain an error
  // message from the CA. On success both the key and certificate are associated
  // with |username|, which must be a canonical email address or the empty
  // string (in which case it will be associated to the device). The |origin|
  // parameter is passed to the CA; it is required by some certificate profiles.
  using CreateGoogleAttestedKeyCallback =
      base::Callback<void(const std::string& certificate_chain,
                          const std::string& server_error_details,
                          AttestationStatus status)>;
  virtual void CreateGoogleAttestedKey(
      const std::string& key_label,
      KeyType key_type,
      KeyUsage key_usage,
      CertificateProfile certificate_profile,
      const std::string& username,
      const std::string& origin,
      const CreateGoogleAttestedKeyCallback& callback) = 0;
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_
