// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/token_manager_client.h"

#include <base/logging.h>
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>

#include "chaps/chaps_proxy.h"
#include "chaps/chaps_utility.h"

using base::FilePath;
using chromeos::SecureBlob;
using std::vector;
using std::string;

namespace chaps {

TokenManagerClient::TokenManagerClient()
  : proxy_(new ChapsProxyImpl()),
    is_connected_(false) {
  CHECK(proxy_);
}

TokenManagerClient::~TokenManagerClient() {
}

bool TokenManagerClient::GetTokenList(const SecureBlob& isolate_credential,
                                      vector<string>* result) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(ERROR) << __func__ << ": Failed to connect to the Chaps daemon.";
    return false;
  }

  vector<uint64_t> slots;
  if (proxy_->GetSlotList(isolate_credential, true, &slots) != CKR_OK) {
    LOG(ERROR) << __func__ << ": GetSlotList failed.";
    return false;
  }

  vector<string> temp_result;
  for (size_t i = 0; i < slots.size(); ++i) {
    FilePath path;
    if (!GetTokenPath(isolate_credential, slots[i], &path)) {
      LOG(ERROR) << __func__ << ": GetTokenPath failed.";
      return false;
    }
    temp_result.push_back(path.value());
  }

  result->swap(temp_result);
  return true;
}

bool TokenManagerClient::OpenIsolate(SecureBlob* isolate_credential,
                                   bool* new_isolate_created) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(ERROR) << __func__ << ": Failed to connect to the Chaps daemon.";
    return false;
  }
  return proxy_->OpenIsolate(isolate_credential, new_isolate_created);
}

void TokenManagerClient::CloseIsolate(const SecureBlob& isolate_credential) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(ERROR) << __func__ << ": Failed to connect to the Chaps daemon.";
    return;
  }
  proxy_->CloseIsolate(isolate_credential);
}

bool TokenManagerClient::LoadToken(const SecureBlob& isolate_credential,
                                   const FilePath& path,
                                   const SecureBlob& auth_data,
                                   const string& label,
                                   int* slot_id) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(ERROR) << __func__ << ": Failed to connect to the Chaps daemon.";
    return false;
  }
  bool result = proxy_->LoadToken(isolate_credential,
                                  path.value(),
                                  auth_data,
                                  label,
                                  PreservedValue<int, uint64_t>(slot_id));
  return result;
}

void TokenManagerClient::UnloadToken(const SecureBlob& isolate_credential,
                                     const FilePath& path) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(ERROR) << __func__ << ": Failed to connect to the Chaps daemon.";
    return;
  }
  proxy_->UnloadToken(isolate_credential, path.value());
}

void TokenManagerClient::ChangeTokenAuthData(const FilePath& path,
                                             const SecureBlob&  old_auth_data,
                                             const SecureBlob& new_auth_data) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(ERROR) << __func__ << ": Failed to connect to the Chaps daemon.";
    return;
  }
  proxy_->ChangeTokenAuthData(path.value(), old_auth_data, new_auth_data);
}

bool TokenManagerClient::GetTokenPath(const SecureBlob& isolate_credential,
                                      int slot_id,
                                      FilePath* path) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(ERROR) << __func__ << ": Failed to connect to the Chaps daemon.";
    return false;
  }
  string tmp;
  bool result = proxy_->GetTokenPath(isolate_credential, slot_id, &tmp);
  *path = FilePath(tmp);
  return result;
}

bool TokenManagerClient::Connect() {
  if (!is_connected_)
    is_connected_ = proxy_->Init();
  return is_connected_;
}

}  // namespace chaps
