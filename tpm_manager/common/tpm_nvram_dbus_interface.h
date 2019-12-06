// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_COMMON_TPM_NVRAM_DBUS_INTERFACE_H_
#define TPM_MANAGER_COMMON_TPM_NVRAM_DBUS_INTERFACE_H_

namespace tpm_manager {

constexpr char kTpmNvramInterface[] = "org.chromium.TpmNvram";

// Methods exported by tpm_manager nvram D-Bus interface.
constexpr char kDefineSpace[] = "DefineSpace";
constexpr char kDestroySpace[] = "DestroySpace";
constexpr char kWriteSpace[] = "WriteSpace";
constexpr char kReadSpace[] = "ReadSpace";
constexpr char kLockSpace[] = "LockSpace";
constexpr char kListSpaces[] = "ListSpaces";
constexpr char kGetSpaceInfo[] = "GetSpaceInfo";

}  // namespace tpm_manager

#endif  // TPM_MANAGER_COMMON_TPM_NVRAM_DBUS_INTERFACE_H_
