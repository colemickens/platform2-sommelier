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

  MOCK_METHOD(void, SetTpm, (Tpm*), (override));
  MOCK_METHOD(bool, Init, (TpmInit*), (override));
  MOCK_METHOD(bool,
              Get,
              (const std::string&, brillo::Blob*),
              (const, override));
  MOCK_METHOD(bool,
              GetByIndex,
              (int, std::string*, brillo::Blob*),
              (const, override));
  MOCK_METHOD(bool, Set, (const std::string&, const brillo::Blob&), (override));
  MOCK_METHOD(bool, Finalize, (), (override));
  MOCK_METHOD(int, Count, (), (const, override));
  MOCK_METHOD(bool, is_secure, (), (const, override));
  MOCK_METHOD(InstallAttributes::Status, status, (), (const, override));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_INSTALL_ATTRIBUTES_H_
