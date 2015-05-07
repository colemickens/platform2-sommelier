// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_CLIENT_TEST_H_
#define TRUNKS_TRUNKS_CLIENT_TEST_H_

#include <string>

#include <base/memory/scoped_ptr.h>

#include "trunks/tpm_generated.h"
#include "trunks/trunks_factory.h"

namespace trunks {

// This class is used to perform integration tests on the TPM. Each public
// method defines a different test to perform.
// NOTE: All these tests require that the TPM be owned, and SRKs exist.
// Example usage:
// TrunksClientTest test;
// CHECK(test.RNGTest());
// CHECK(test.SimplePolicyTest());
class TrunksClientTest {
 public:
  TrunksClientTest();
  // Takes ownership of factory.
  explicit TrunksClientTest(scoped_ptr<TrunksFactory> factory);
  virtual ~TrunksClientTest();

  // This test verifies that the Random Number Generator on the TPM is working
  // correctly.
  bool RNGTest();

  // This test verifies that we can create an unrestricted RSA signing key and
  // use it to sign arbitrary data.
  bool SignTest();

  // This test verfifies that we can create an unrestricted RSA decryption key
  // and use it to encrypt and decrypt arbitrary data.
  bool DecryptTest();

  // This test verifies that we can import a RSA key into the TPM and use it
  // to encrypt and decrypt some data.
  bool ImportTest();

  // This test verifies that we can change a key's authorization data and
  // still use it to encrypt/decrypt data.
  bool AuthChangeTest();

  // This test sets up a PolicySession with the PolicyAuthValue assertion.
  // This policy is then used to create a key and use it to sign/verify and
  // encrypt/decrypt.
  bool SimplePolicyTest();

  // This test performs a simple PCR extension and then reads the value in the
  // PCR to verify if it is correct.
  // NOTE: PCR banks need to be configured for this test to succeed. Normally
  // this is done by the platform firmware.
  bool PCRTest();

  // This test verfies that we can create, write, read, lock and delete
  // NV spaces in the TPM.
  // NOTE: This test needs the |owner_password| to work.
  bool NvramTest(const std::string& owner_password);

 private:
  // This method verifies that plaintext == decrypt(encrypt(plaintext)) using
  // a given key.
  // TODO(usanghi): Remove |session| argument once we can support multiple
  // sessions.
  bool PerformRSAEncrpytAndDecrpyt(TPM_HANDLE key_handle,
                                   const std::string& key_authorization,
                                   HmacSession* session);

  // Factory for instantiation of Tpm classes
  scoped_ptr<TrunksFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(TrunksClientTest);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_CLIENT_TEST_H_
