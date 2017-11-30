// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_TPM_UTIL_H_
#define TPM_MANAGER_SERVER_TPM_UTIL_H_

namespace tpm_manager {

#define TPM_LOG(severity, result) \
  LOG(severity) << "TPM error 0x" << std::hex << result \
                << " (" << Trspi_Error_String(result) << "): "

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_UTIL_H_
