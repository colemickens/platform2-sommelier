// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OOBE_CONFIG_TPM_CRYPTO_IMPL_H_
#define OOBE_CONFIG_TPM_CRYPTO_IMPL_H_

#include <string>

#include <base/macros.h>
#include <brillo/secure_blob.h>


// TODO(zentaro): These are just stub implementations of TpmCrypto and
// TpmCryptoImpl. They will be replaced by the real implementations after
// CL:1308515 lands. This is done by CL:1347143.
namespace tpmcrypto {

class TpmCrypto {
 public:
  virtual ~TpmCrypto() = default;

  virtual bool Encrypt(const brillo::SecureBlob& data,
                       std::string* encrypted_data) = 0;

  virtual bool Decrypt(const std::string& encrypted_data,
                       brillo::SecureBlob* data) = 0;

 protected:
  TpmCrypto() = default;

  DISALLOW_COPY_AND_ASSIGN(TpmCrypto);
};

class TpmCryptoImpl : public TpmCrypto {
 public:
  TpmCryptoImpl() = default;
  ~TpmCryptoImpl() override = default;

  bool Encrypt(const brillo::SecureBlob& data,
               std::string* encrypted_data) override WARN_UNUSED_RESULT {
    NOTREACHED();
    return false;
  }

  bool Decrypt(const std::string& encrypted_data,
               brillo::SecureBlob* data) override WARN_UNUSED_RESULT {
    NOTREACHED();
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(TpmCryptoImpl);
};


}  // namespace tpmcrypto

#endif  // OOBE_CONFIG_TPM_CRYPTO_IMPL_H_
