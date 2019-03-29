//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "tpm_manager/server/tpm_connection.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/stl_util.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>  // NOLINT(build/include_alpha)

#include "tpm_manager/server/tpm_util.h"

namespace {

const int kTpmConnectRetries = 10;
const int kTpmConnectIntervalMs = 100;

}  // namespace

namespace tpm_manager {

TpmConnection::TpmConnection(): connection_type_(kConnectWithoutAuth) {}

TpmConnection::TpmConnection(const std::string& owner_password)
    : owner_password_(owner_password),
      connection_type_(kConnectWithPassword) {}

TpmConnection::TpmConnection(const AuthDelegate& owner_delegate)
    : owner_delegate_(owner_delegate),
      connection_type_(kConnectWithDelegate) {}

TSS_HCONTEXT TpmConnection::GetContext() {
  if (!ConnectContextIfNeeded()) {
    return 0;
  }
  return context_.value();
}

TSS_HTPM TpmConnection::GetTpm() {
  if (!ConnectContextIfNeeded()) {
    return 0;
  }
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (TPM_ERROR(result =
                    Tspi_Context_GetTpmObject(context_.value(), &tpm_handle))) {
    TPM_LOG(ERROR, result) << "Error getting a handle to the TPM.";
    return 0;
  }
  return tpm_handle;
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
  if (context_.value() == 0) {
    LOG(ERROR) << "Unexpected NULL context.";
    return false;
  }

  // If we don't need authorization, we're done.
  if (connection_type_ == kConnectWithoutAuth) {
    return true;
  }

  TSS_HTPM tpm_handle;
  if (TPM_ERROR(result =
                    Tspi_Context_GetTpmObject(context_.value(), &tpm_handle))) {
    TPM_LOG(ERROR, result) << "Error getting a handle to the TPM.";
    context_.reset();
    return false;
  }

  TSS_HPOLICY tpm_usage_policy;
  if (TPM_ERROR(result = Tspi_GetPolicyObject(tpm_handle, TSS_POLICY_USAGE,
                                              &tpm_usage_policy))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_GetPolicyObject";
    context_.reset();
    return false;
  }

  const std::string& secret = connection_type_ == kConnectWithPassword ?
      owner_password_ : owner_delegate_.secret();
  std::vector<BYTE> secret_data(secret.begin(), secret.end());
  if (TPM_ERROR(result = Tspi_Policy_SetSecret(tpm_usage_policy,
                                               TSS_SECRET_MODE_PLAIN,
                                               secret_data.size(),
                                               secret_data.data()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Policy_SetSecret";
    context_.reset();
    return false;
  }

  if (connection_type_ == kConnectWithPassword) {
    return true;
  }

  // For connection with owner delegate, we also need to set attribute data.
  std::vector<BYTE> delegate_blob(owner_delegate_.blob().begin(),
                                  owner_delegate_.blob().end());
  if (TPM_ERROR(result = Tspi_SetAttribData(
      tpm_usage_policy,
      TSS_TSPATTRIB_POLICY_DELEGATION_INFO,
      TSS_TSPATTRIB_POLDEL_OWNERBLOB,
      delegate_blob.size(),
      delegate_blob.data()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_SetAttribData";
    return false;
  }

  return true;
}

}  // namespace tpm_manager
