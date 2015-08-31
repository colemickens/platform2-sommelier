// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/server/tpm_connection.h"

#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>  // NOLINT(build/include_alpha)

namespace {

const int kTpmConnectRetries = 10;
const int kTpmConnectIntervalMs = 100;

}  // namespace

namespace tpm_manager {

#define TPM_LOG(severity, result) \
  LOG(severity) << "TPM error 0x" << std::hex << result \
                << " (" << Trspi_Error_String(result) << "): "

TSS_HTPM TpmConnection::GetTpm() {
  if (!ConnectContextIfNeeded()) {
    return 0;
  }
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (TPM_ERROR(result = Tspi_Context_GetTpmObject(context_.value(),
                                                   &tpm_handle))) {
    TPM_LOG(ERROR, result) << "Error getting a handle to the TPM.";
    return 0;
  }
  return tpm_handle;
}

TSS_HCONTEXT TpmConnection::GetContext() {
  if (!ConnectContextIfNeeded()) {
    return 0;
  }
  return context_.value();
}

bool TpmConnection::ConnectContextIfNeeded() {
  if (context_.value() != 0) {
    return true;
  }
  TSS_RESULT result;
  if (TPM_ERROR(result = Tspi_Context_Create(context_.ptr()))) {
    TPM_LOG(ERROR, result) << "Error connecting to TPM.";
    return false;
  }
  // We retry on failure. It might be that tcsd is starting up.
  for (int i = 0; i < kTpmConnectRetries; i++) {
    if (TPM_ERROR(result = Tspi_Context_Connect(context_, nullptr))) {
      if (ERROR_CODE(result) == TSS_E_COMM_FAILURE) {
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kTpmConnectIntervalMs));
      } else {
        TPM_LOG(ERROR, result) << "Error connecting to TPM.";
        return false;
      }
    } else {
      break;
    }
  }
  return (context_.value() != 0);
}

}  // namespace tpm_manager
