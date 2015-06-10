// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_COMMON_DBUS_INTERFACE_H_
#define TPM_MANAGER_COMMON_DBUS_INTERFACE_H_

namespace tpm_manager {

constexpr char kTpmManagerInterface[] = "org.chromium.TpmManager";
constexpr char kTpmManagerServicePath[] = "/org/chromium/TpmManager";
constexpr char kTpmManagerServiceName[] = "org.chromium.TpmManager";

// Methods exported by tpm_manager.
constexpr char kGetTpmStatus[] = "GetTpmStatus";
constexpr char kTakeOwnership[] = "TakeOwnership";

}  // namespace tpm_manager

#endif  // TPM_MANAGER_COMMON_DBUS_INTERFACE_H_
