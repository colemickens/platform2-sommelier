// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/login_event_client.h"

#include <base/logging.h>
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>

#include "chaps/chaps_proxy.h"
#include "chaps/chaps_utility.h"

using chromeos::SecureBlob;
using std::string;

namespace chaps {

LoginEventClient::LoginEventClient()
  : proxy_(new ChapsProxyImpl()),
    is_connected_(false) {
  CHECK(proxy_);
}

LoginEventClient::~LoginEventClient() {
  delete proxy_;
}

bool LoginEventClient::OpenIsolate(SecureBlob* isolate_credential) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(WARNING) << "Failed to connect to the Chaps daemon. "
                 << "Login notification will not be sent.";
    return false;
  }
  return proxy_->OpenIsolate(isolate_credential);
}

void LoginEventClient::CloseIsolate(const SecureBlob& isolate_credential) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(WARNING) << "Failed to connect to the Chaps daemon. "
                 << "Logout notification will not be sent.";
    return;
  }
  proxy_->CloseIsolate(isolate_credential);
}

bool LoginEventClient::LoadToken(const SecureBlob& isolate_credential,
                                 const string& path,
                                 const uint8_t* auth_data,
                                 size_t auth_data_length,
                                 int* slot_id) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(WARNING) << "Failed to connect to the Chaps daemon. "
                 << "Load Token notification will not be sent.";
    return false;
  }
  SecureBlob auth_data_blob(auth_data, auth_data_length);
  return proxy_->LoadToken(isolate_credential, path, auth_data_blob, slot_id);
}

void LoginEventClient::UnloadToken(const SecureBlob& isolate_credential,
                                   const string& path) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(WARNING) << "Failed to connect to the Chaps daemon. "
                 << "Unload Token notification will not be sent.";
    return;
  }
  proxy_->UnloadToken(isolate_credential, path);
}

void LoginEventClient::ChangeTokenAuthData(const string& path,
                                           const uint8_t* old_auth_data,
                                           size_t old_auth_data_length,
                                           const uint8_t* new_auth_data,
                                           size_t new_auth_data_length) {
  CHECK(proxy_);
  if (!Connect()) {
    LOG(WARNING) << "Failed to connect to the Chaps daemon. "
                 << "Change authorization data notification will not be sent.";
    return;
  }
  SecureBlob old_auth_data_blob(old_auth_data, old_auth_data_length);
  SecureBlob new_auth_data_blob(new_auth_data, new_auth_data_length);
  proxy_->ChangeTokenAuthData(path, old_auth_data_blob, new_auth_data_blob);
}

bool LoginEventClient::Connect() {
  if (!is_connected_)
    is_connected_ = proxy_->Init();
  return is_connected_;
}

}  // namespace chaps
