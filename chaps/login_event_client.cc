// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/login_event_client.h"

#include <vector>

#include <base/logging.h>
#include <chromeos/utility.h>

#include "chaps/chaps_proxy.h"
#include "chaps/chaps_utility.h"

using std::string;
using std::vector;

namespace chaps {

LoginEventClient::LoginEventClient() : proxy_(NULL) {}

LoginEventClient::~LoginEventClient() {
  delete proxy_;
}

bool LoginEventClient::Init() {
  proxy_ = new ChapsProxyImpl();
  CHECK(proxy_);
  return proxy_->Init();
}

void LoginEventClient::FireLoginEvent(const string& path,
                                      const uint8_t* auth_data,
                                      size_t auth_data_length) {
  CHECK(proxy_);
  // TODO(dkrahn): Use SecureBlob; see crosbug.com/27681.
  vector<uint8_t> auth_data_vector(auth_data, auth_data + auth_data_length);
  proxy_->FireLoginEvent(path, auth_data_vector);
  chromeos::SecureMemset(&auth_data_vector[0], 0, auth_data_length);
}

void LoginEventClient::FireLogoutEvent(const string& path) {
  CHECK(proxy_);
  proxy_->FireLogoutEvent(path);
}

void LoginEventClient::FireChangeAuthDataEvent(
    const string& path,
    const uint8_t* old_auth_data,
    size_t old_auth_data_length,
    const uint8_t* new_auth_data,
    size_t new_auth_data_length) {
  CHECK(proxy_);
  // TODO(dkrahn): Use SecureBlob; see crosbug.com/27681.
  vector<uint8_t> old_auth_data_vector(old_auth_data,
                                       old_auth_data + old_auth_data_length);
  vector<uint8_t> new_auth_data_vector(new_auth_data,
                                       new_auth_data + new_auth_data_length);
  proxy_->FireChangeAuthDataEvent(path,
                                  old_auth_data_vector,
                                  new_auth_data_vector);
  chromeos::SecureMemset(&old_auth_data_vector[0], 0, old_auth_data_length);
  chromeos::SecureMemset(&new_auth_data_vector[0], 0, new_auth_data_length);
}

}  // namespace chaps
