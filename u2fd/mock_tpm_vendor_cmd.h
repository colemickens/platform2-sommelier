// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_MOCK_TPM_VENDOR_CMD_H_
#define U2FD_MOCK_TPM_VENDOR_CMD_H_

#include "u2fd/tpm_vendor_cmd.h"

#include <string>

#include <gmock/gmock.h>
#include <trunks/cr50_headers/u2f.h>

namespace u2f {

class MockTpmVendorCommandProxy : public TpmVendorCommandProxy {
 public:
  MOCK_METHOD1(SetU2fVendorMode, uint32_t(uint8_t));
  MOCK_METHOD2(SendU2fGenerate,
               uint32_t(const U2F_GENERATE_REQ&, U2F_GENERATE_RESP*));
  MOCK_METHOD2(SendU2fSign, uint32_t(const U2F_SIGN_REQ&, U2F_SIGN_RESP*));
  MOCK_METHOD2(SendU2fAttest,
               uint32_t(const U2F_ATTEST_REQ&, U2F_ATTEST_RESP*));
  MOCK_METHOD1(GetG2fCertificate, uint32_t(std::string*));
};

}  // namespace u2f

#endif  // U2FD_MOCK_TPM_VENDOR_CMD_H_
