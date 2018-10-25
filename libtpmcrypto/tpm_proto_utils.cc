// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libtpmcrypto/tpm_proto_utils.h"

#include <utility>

#include <base/logging.h>
#include <brillo/secure_blob.h>

using brillo::SecureBlob;

namespace tpmcrypto {

bool CreateSerializedTpmCryptoProto(const SecureBlob& sealed_key,
                                    const SecureBlob& iv,
                                    const SecureBlob& tag,
                                    const SecureBlob& encrypted_data,
                                    std::string* serialized) {
  TpmEncryptedData encrypted_pb;
  encrypted_pb.set_sealed_key(sealed_key.data(), sealed_key.size());
  encrypted_pb.set_iv(iv.data(), iv.size());
  encrypted_pb.set_encrypted_data(encrypted_data.data(), encrypted_data.size());
  encrypted_pb.set_tag(tag.data(), tag.size());

  if (!encrypted_pb.SerializeToString(serialized)) {
    LOG(ERROR) << "Could not serialize TpmEncryptedData proto to string.";
    return false;
  }

  return true;
}

bool ParseTpmCryptoProto(const std::string& serialized,
                         SecureBlob* sealed_key,
                         SecureBlob* iv,
                         SecureBlob* tag,
                         SecureBlob* encrypted_data) {
  TpmEncryptedData encrypted_pb;
  if (!encrypted_pb.ParseFromString(serialized)) {
    LOG(ERROR) << "Could not decrypt data as it was not a TpmEncryptedData "
               << "protobuf";
    return false;
  }

  SecureBlob tmp_sealed_key(encrypted_pb.sealed_key().begin(),
                            encrypted_pb.sealed_key().end());
  SecureBlob tmp_iv(encrypted_pb.iv().begin(), encrypted_pb.iv().end());
  SecureBlob tmp_tag(encrypted_pb.tag().begin(), encrypted_pb.tag().end());
  SecureBlob tmp_encrypted_data(encrypted_pb.encrypted_data().begin(),
                                encrypted_pb.encrypted_data().end());

  *sealed_key = std::move(tmp_sealed_key);
  *iv = std::move(tmp_iv);
  *tag = std::move(tmp_tag);
  *encrypted_data = std::move(tmp_encrypted_data);

  return true;
}

}  // namespace tpmcrypto
