// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_UTILS_H_
#define WEBSERVER_WEBSERVD_UTILS_H_

#include <memory>
#include <string>
#include <vector>
#include <openssl/ossl_typ.h>

#include <base/time/time.h>
#include <chromeos/secure_blob.h>

namespace webservd {

// Creates a new X509 certificate.
std::unique_ptr<X509, void(*)(X509*)> CreateCertificate(
    int serial_number,
    const base::TimeDelta& cert_expiration,
    const std::string& common_name);

// Generates an RSA public-private key pair of the specified strength.
std::unique_ptr<RSA, void(*)(RSA*)> GenerateRSAKeyPair(int key_length_bits);

// Serializes a private key from the key pair into a PEM string and returns
// it as a binary blob.
chromeos::SecureBlob StoreRSAPrivateKey(RSA* rsa_key_pair);

// Serializes an X509 certificate using PEM format.
chromeos::Blob StoreCertificate(X509* cert);

// Same as openssl x509 -fingerprint -sha256.
chromeos::Blob GetSha256Fingerprint(X509* cert);

// Creates a socket bound to a specified network interface.
// Returns a socket file descriptor or -1 on error.
int CreateNetworkInterfaceSocket(const std::string& if_name);

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_UTILS_H_
