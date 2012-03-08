// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/login_event_client.h"

#include <base/logging.h>

#include "chaps/chaps_proxy.h"
#include "chaps/chaps_utility.h"

using std::string;

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

void LoginEventClient::FireLoginEvent(const std::string& path,
                                      const std::string& auth_data) {
  CHECK(proxy_);
  proxy_->FireLoginEvent(path, ConvertByteStringToVector(auth_data));
}

void LoginEventClient::FireLogoutEvent(const std::string& path) {
  CHECK(proxy_);
  proxy_->FireLogoutEvent(path);
}

void LoginEventClient::FireChangeAuthDataEvent(
    const std::string& path,
    const std::string& old_auth_data,
    const std::string& new_auth_data) {
  CHECK(proxy_);
  proxy_->FireChangeAuthDataEvent(path,
                                  ConvertByteStringToVector(old_auth_data),
                                  ConvertByteStringToVector(new_auth_data));
}

}  // namespace chaps
