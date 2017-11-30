// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS CODE IS GENERATED.

#ifndef ATTESTATION_COMMON_PRINT_COMMON_PROTO_H_
#define ATTESTATION_COMMON_PRINT_COMMON_PROTO_H_

#include <string>

#include "attestation/common/common.pb.h"

namespace attestation {

std::string GetProtoDebugStringWithIndent(KeyType value, int indent_size);
std::string GetProtoDebugString(KeyType value);
std::string GetProtoDebugStringWithIndent(KeyUsage value, int indent_size);
std::string GetProtoDebugString(KeyUsage value);
std::string GetProtoDebugStringWithIndent(CertificateProfile value,
                                          int indent_size);
std::string GetProtoDebugString(CertificateProfile value);
std::string GetProtoDebugStringWithIndent(const Quote& value, int indent_size);
std::string GetProtoDebugString(const Quote& value);
std::string GetProtoDebugStringWithIndent(const EncryptedData& value,
                                          int indent_size);
std::string GetProtoDebugString(const EncryptedData& value);
std::string GetProtoDebugStringWithIndent(const SignedData& value,
                                          int indent_size);
std::string GetProtoDebugString(const SignedData& value);
std::string GetProtoDebugStringWithIndent(
    const EncryptedIdentityCredential& value,
    int indent_size);
std::string GetProtoDebugString(const EncryptedIdentityCredential& value);

}  // namespace attestation

#endif  // ATTESTATION_COMMON_PRINT_COMMON_PROTO_H_
