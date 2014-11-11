// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/security_delegate.h"

namespace privetd {

namespace {

class SecurityDelegateImpl : public SecurityDelegate {
 public:
  SecurityDelegateImpl() {}
  ~SecurityDelegateImpl() override {}

  // SecurityDelegate methods
  std::string CreateAccessToken(AuthScope scope) const override {
    return "FakeToken";
  }
  AuthScope GetScopeFromAccessToken(
      const std::string& token,
      const base::Time& expire_before) const override {
    return AuthScope::kOwner;
  }
  std::vector<PairingType> GetPairingTypes() const override {
    return {PairingType::kEmbeddedCode};
  }
  bool IsValidPairingCode(const std::string& auth_code) const override {
    return true;
  }
};

}  // namespace

SecurityDelegate::SecurityDelegate() {
}

SecurityDelegate::~SecurityDelegate() {
}

// static
std::unique_ptr<SecurityDelegate> SecurityDelegate::CreateDefault() {
  return std::unique_ptr<SecurityDelegate>(new SecurityDelegateImpl);
}

}  // namespace privetd
