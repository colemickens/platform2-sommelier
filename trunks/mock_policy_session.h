// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_MOCK_POLICY_SESSION_H_
#define TRUNKS_MOCK_POLICY_SESSION_H_

#include "trunks/policy_session.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>

namespace trunks {

class MockPolicySession : public PolicySession {
 public:
  MockPolicySession();
  ~MockPolicySession() override;

  MOCK_METHOD0(GetDelegate, AuthorizationDelegate*());
  MOCK_METHOD3(StartBoundSession, TPM_RC(
      TPMI_DH_ENTITY bind_entity,
      const std::string& bind_authorization_value,
      bool enable_encryption));
  MOCK_METHOD1(StartUnboundSession, TPM_RC(bool enable_encryption));
  MOCK_METHOD1(GetDigest, TPM_RC(std::string*));
  MOCK_METHOD1(PolicyOR, TPM_RC(const std::vector<std::string>&));
  MOCK_METHOD2(PolicyPCR, TPM_RC(uint32_t, const std::string&));
  MOCK_METHOD1(PolicyCommandCode, TPM_RC(TPM_CC));
  MOCK_METHOD0(PolicyAuthValue, TPM_RC());
  MOCK_METHOD1(SetEntityAuthorizationValue, void(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPolicySession);
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_POLICY_SESSION_H_
