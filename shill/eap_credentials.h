// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_EAP_CREDENTIALS_H_
#define SHILL_EAP_CREDENTIALS_H_

#include <string>
#include <vector>

namespace shill {

struct EapCredentials {
  EapCredentials() : use_system_cas(true) {}
  // Who we identify ourselves as to the EAP authenticator.
  std::string identity;
  // The outer or only EAP authetnication type.
  std::string eap;
  // The inner EAP authentication type.
  std::string inner_eap;
  // When there is an inner EAP type, use this identity for the outer.
  std::string anonymous_identity;
  // Filename of the client certificate.
  std::string client_cert;
  // Locator for the client certificate within the security token.
  std::string cert_id;
  // Filename of the client private key.
  std::string private_key;
  // Password for decrypting the client private key file.
  std::string private_key_password;
  // Locator for the client private key within the security token.
  std::string key_id;
  // Filename of the certificate authority (CA) certificate.
  std::string ca_cert;
  // Locator for the CA certificate within the security token.
  std::string ca_cert_id;
  // Locator for the CA certificate within the user NSS database.
  std::string ca_cert_nss;
  // Raw PEM contents of the CA certificate.
  std::string ca_cert_pem;
  // If true, use the system-wide CA database to authenticate the remote.
  bool use_system_cas;
  // PIN code for accessing the security token.
  std::string pin;
  // Password to use for EAP methods which require one.
  std::string password;
  // Key management algorithm to use after EAP succeeds.
  std::string key_management;
  // If non-empty, string to match remote subject against before connecting.
  std::string subject_match;
  // List of subject names reported by remote entity during TLS setup.
  std::vector<std::string> remote_certification;
};

}  // namespace shill

#endif  // SHILL_EAP_CREDENTIALS_H_

