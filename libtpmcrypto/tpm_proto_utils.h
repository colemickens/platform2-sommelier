// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPMCRYPTO_TPM_PROTO_UTILS_H_
#define TPMCRYPTO_TPM_PROTO_UTILS_H_

#include <string>

#include "libtpmcrypto/tpm_encrypted_data.pb.h"

namespace brillo {
class SecureBlob;
}  // namespace brillo

namespace tpmcrypto {

bool CreateSerializedTpmCryptoProto(const brillo::SecureBlob& sealed_key,
                                    const brillo::SecureBlob& iv,
                                    const brillo::SecureBlob& tag,
                                    const brillo::SecureBlob& encrypted_data,
                                    std::string* serialized);

bool ParseTpmCryptoProto(const std::string& serialized,
                         brillo::SecureBlob* sealed_key,
                         brillo::SecureBlob* iv,
                         brillo::SecureBlob* tag,
                         brillo::SecureBlob* encrypted_data);

}  // namespace tpmcrypto

#endif  // TPMCRYPTO_TPM_PROTO_UTILS_H_
