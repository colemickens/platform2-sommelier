// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/null_auth_delegate.h"

namespace trunks {

NullAuthDelegate::NullAuthDelegate() {}

NullAuthDelegate::~NullAuthDelegate() {}

bool NullAuthDelegate::GetCommandAuthorization(
    const std::string& command_hash,
    std::string* authorization) {
  return true;
}

bool NullAuthDelegate::CheckResponseAuthorization(
    const std::string& response_hash,
    const std::string& authorization) {
  return true;
}

bool NullAuthDelegate::EncryptCommandParameter(std::string* parameter) {
  return true;
}

bool NullAuthDelegate::DecryptResponseParameter(std::string* parameter) {
  return true;
}

}  // namespace trunks
