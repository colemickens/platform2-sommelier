// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/null_authorization_delegate.h"

namespace trunks {

NullAuthorizationDelegate::NullAuthorizationDelegate() {}

NullAuthorizationDelegate::~NullAuthorizationDelegate() {}

bool NullAuthorizationDelegate::GetCommandAuthorization(
    const std::string& command_hash,
    std::string* authorization) {
  return true;
}

bool NullAuthorizationDelegate::CheckResponseAuthorization(
    const std::string& response_hash,
    const std::string& authorization) {
  return true;
}

bool NullAuthorizationDelegate::EncryptCommandParameter(
    std::string* parameter) {
  return true;
}

bool NullAuthorizationDelegate::DecryptResponseParameter(
    std::string* parameter) {
  return true;
}

}  // namespace trunks
