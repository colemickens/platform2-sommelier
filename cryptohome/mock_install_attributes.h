// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_INSTALL_ATTRIBUTES_H_
#define CRYPTOHOME_MOCK_INSTALL_ATTRIBUTES_H_

#include "cryptohome/install_attributes.h"

#include <string>

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>

#include "cryptohome/tpm.h"
#include "cryptohome/tpm_init.h"

namespace cryptohome {

class MockInstallAttributes : public InstallAttributes {
 public:
  MockInstallAttributes();
  virtual ~MockInstallAttributes();

  MOCK_METHOD1(SetTpm, void(Tpm*));  // NOLINT(readability/function)
  MOCK_METHOD1(Init, bool(TpmInit*));  // NOLINT(readability/function)
  MOCK_CONST_METHOD0(IsReady, bool());
  MOCK_CONST_METHOD2(Get, bool(const std::string&, brillo::Blob*));
  MOCK_CONST_METHOD3(GetByIndex, bool(int, std::string*, brillo::Blob*));
  MOCK_METHOD2(Set, bool(const std::string&, const brillo::Blob&));
  MOCK_METHOD0(Finalize, bool());
  MOCK_CONST_METHOD0(Count, int());
  MOCK_CONST_METHOD0(is_secure, bool());
  MOCK_CONST_METHOD0(status, InstallAttributes::Status());
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_INSTALL_ATTRIBUTES_H_
