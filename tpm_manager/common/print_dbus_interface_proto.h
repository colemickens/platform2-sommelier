// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS CODE IS GENERATED.

#ifndef TPM_MANAGER_COMMON_PRINT_DBUS_INTERFACE_PROTO_H_
#define TPM_MANAGER_COMMON_PRINT_DBUS_INTERFACE_PROTO_H_

#include <string>

#include "tpm_manager/common/dbus_interface.pb.h"

namespace tpm_manager {

std::string GetProtoDebugStringWithIndent(TpmManagerStatus value,
                                          int indent_size);
std::string GetProtoDebugString(TpmManagerStatus value);
std::string GetProtoDebugStringWithIndent(const GetTpmStatusRequest& value,
                                          int indent_size);
std::string GetProtoDebugString(const GetTpmStatusRequest& value);
std::string GetProtoDebugStringWithIndent(const GetTpmStatusReply& value,
                                          int indent_size);
std::string GetProtoDebugString(const GetTpmStatusReply& value);
std::string GetProtoDebugStringWithIndent(const TakeOwnershipRequest& value,
                                          int indent_size);
std::string GetProtoDebugString(const TakeOwnershipRequest& value);
std::string GetProtoDebugStringWithIndent(const TakeOwnershipReply& value,
                                          int indent_size);
std::string GetProtoDebugString(const TakeOwnershipReply& value);

}  // namespace tpm_manager

#endif  // TPM_MANAGER_COMMON_PRINT_DBUS_INTERFACE_PROTO_H_
