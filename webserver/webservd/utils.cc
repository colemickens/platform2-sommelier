// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webserver/webservd/utils.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

namespace webservd {

std::unique_ptr<X509, void(*)(X509*)> CreateCertificate(
    int serial_number,
    const base::TimeDelta& cert_expiration,
    const std::string& common_name) {
  auto cert = std::unique_ptr<X509, void(*)(X509*)>{X509_new(), X509_free};
  CHECK(cert.get());
  X509_set_version(cert.get(), 2);  // Using X.509 v3 certificate...

  // Set certificate properties...
  ASN1_INTEGER* sn = X509_get_serialNumber(cert.get());
  ASN1_INTEGER_set(sn, serial_number);
  X509_gmtime_adj(X509_get_notBefore(cert.get()), 0);
  X509_gmtime_adj(X509_get_notAfter(cert.get()), cert_expiration.InSeconds());

  // The issuer is the same as the subject, since this cert is self-signed.
  X509_NAME* name = X509_get_subject_name(cert.get());
  if (!common_name.empty()) {
    X509_NAME_add_entry_by_txt(
        name, "CN", MBSTRING_UTF8,
        reinterpret_cast<const unsigned char*>(common_name.c_str()),
        common_name.length(), -1, 0);
  }
  X509_set_issuer_name(cert.get(), name);
  return cert;
}

std::unique_ptr<RSA, void(*)(RSA*)> GenerateRSAKeyPair(int key_length_bits) {
  // Create RSA key pair.
  auto rsa_key_pair = std::unique_ptr<RSA, void(*)(RSA*)>{RSA_new(), RSA_free};
  CHECK(rsa_key_pair.get());
  auto big_num = std::unique_ptr<BIGNUM, void(*)(BIGNUM*)>{BN_new(), BN_free};
  CHECK(big_num.get());
  CHECK(BN_set_word(big_num.get(), 65537));
  CHECK(RSA_generate_key_ex(rsa_key_pair.get(), key_length_bits, big_num.get(),
                            nullptr));
  return rsa_key_pair;
}

chromeos::SecureBlob StoreRSAPrivateKey(RSA* rsa_key_pair) {
  auto bio =
      std::unique_ptr<BIO, void(*)(BIO*)>{BIO_new(BIO_s_mem()), BIO_vfree};
  CHECK(bio);
  CHECK(PEM_write_bio_RSAPrivateKey(bio.get(), rsa_key_pair, nullptr, nullptr,
                                    0, nullptr, nullptr));
  uint8_t* buffer = nullptr;
  size_t size = BIO_get_mem_data(bio.get(), &buffer);
  CHECK_GT(size, 0u);
  CHECK(buffer);
  chromeos::SecureBlob key_blob(buffer, size);
  chromeos::SecureMemset(buffer, 0, size);
  return key_blob;
}

chromeos::Blob StoreCertificate(X509* cert) {
  auto bio =
      std::unique_ptr<BIO, void(*)(BIO*)>{BIO_new(BIO_s_mem()), BIO_vfree};
  CHECK(bio);
  CHECK(PEM_write_bio_X509(bio.get(), cert));
  uint8_t* buffer = nullptr;
  size_t size = BIO_get_mem_data(bio.get(), &buffer);
  CHECK_GT(size, 0u);
  CHECK(buffer);
  return chromeos::Blob(buffer, buffer + size);
}

// Same as openssl x509 -fingerprint -sha256.
chromeos::Blob GetSha256Fingerprint(X509* cert) {
  chromeos::Blob fingerprint(256 / 8);
  uint32_t len = 0;
  CHECK(X509_digest(cert, EVP_sha256(), fingerprint.data(), &len));
  CHECK_EQ(len, fingerprint.size());
  return fingerprint;
}

}  // namespace webservd
