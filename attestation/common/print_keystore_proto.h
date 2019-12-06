// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS CODE IS GENERATED.
// Generated with command:
// ./proto_print.py --subdir common --proto-include attestation/proto_bindings
// ../../system_api/dbus/attestation/keystore.proto

#ifndef ATTESTATION_COMMON_PRINT_KEYSTORE_PROTO_H_
#define ATTESTATION_COMMON_PRINT_KEYSTORE_PROTO_H_

#include <string>

#include "attestation/proto_bindings/keystore.pb.h"

namespace attestation {

std::string GetProtoDebugStringWithIndent(KeyType value, int indent_size);
std::string GetProtoDebugString(KeyType value);
std::string GetProtoDebugStringWithIndent(KeyUsage value, int indent_size);
std::string GetProtoDebugString(KeyUsage value);

}  // namespace attestation

#endif  // ATTESTATION_COMMON_PRINT_KEYSTORE_PROTO_H_
