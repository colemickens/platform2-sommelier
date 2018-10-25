// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <brillo/secure_blob.h>
#include <gtest/gtest.h>

#include "libtpmcrypto/tpm_proto_utils.h"

using brillo::SecureBlob;

namespace tpmcrypto {

TEST(TpmProtoUtilsTest, RoundTrip) {
  SecureBlob sealed_key("sealed_key");
  SecureBlob iv("iv");
  SecureBlob tag("tag");
  SecureBlob encrypted_data("encrypted_data");

  // Serialize the blobs into a proto.
  std::string serialized;
  ASSERT_TRUE(CreateSerializedTpmCryptoProto(sealed_key, iv, tag,
                                             encrypted_data, &serialized));

  // Parse the blobs out of the proto.
  SecureBlob parsed_sealed_key;
  SecureBlob parsed_iv;
  SecureBlob parsed_tag;
  SecureBlob parsed_encrypted_data;
  ASSERT_TRUE(ParseTpmCryptoProto(serialized, &parsed_sealed_key, &parsed_iv,
                                  &parsed_tag, &parsed_encrypted_data));

  // Verify the blobs that came back are the same as the ones put in.
  EXPECT_EQ(sealed_key, parsed_sealed_key);
  EXPECT_EQ(iv, parsed_iv);
  EXPECT_EQ(tag, parsed_tag);
  EXPECT_EQ(encrypted_data, parsed_encrypted_data);
}

TEST(TpmProtoUtilsTest, EmptyFields) {
  SecureBlob sealed_key;
  SecureBlob iv;
  SecureBlob tag;
  SecureBlob encrypted_data;

  // Serialize the blobs into a proto, then parse them back out.
  std::string serialized;
  ASSERT_TRUE(CreateSerializedTpmCryptoProto(sealed_key, iv, tag,
                                             encrypted_data, &serialized));
  ASSERT_TRUE(
      ParseTpmCryptoProto(serialized, &sealed_key, &iv, &tag, &encrypted_data));

  // Verify the results are empty.
  EXPECT_TRUE(sealed_key.empty());
  EXPECT_TRUE(iv.empty());
  EXPECT_TRUE(tag.empty());
  EXPECT_TRUE(encrypted_data.empty());
}

}  // namespace tpmcrypto
