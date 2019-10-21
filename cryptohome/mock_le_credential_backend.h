// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_LE_CREDENTIAL_BACKEND_H_
#define CRYPTOHOME_MOCK_LE_CREDENTIAL_BACKEND_H_

#include "cryptohome/le_credential_backend.h"

#include <map>
#include <string>
#include <vector>

#include <brillo/secure_blob.h>
#include <dbus/cryptohome/dbus-constants.h>
#include <gmock/gmock.h>

namespace cryptohome {

class MockLECredentialBackend : public LECredentialBackend {
 public:
  MockLECredentialBackend() = default;

  virtual ~MockLECredentialBackend() = default;

  MOCK_METHOD(bool, Reset, (std::vector<uint8_t>*), (override));
  MOCK_METHOD(bool, IsSupported, (), (override));

  MOCK_METHOD(bool,
              InsertCredential,
              (const uint64_t,
               const std::vector<std::vector<uint8_t>>&,
               const brillo::SecureBlob&,
               const brillo::SecureBlob&,
               const brillo::SecureBlob&,
               (const std::map<uint32_t, uint32_t>&),
               const ValidPcrCriteria&,
               std::vector<uint8_t>*,
               std::vector<uint8_t>*,
               std::vector<uint8_t>*),
              (override));

  MOCK_METHOD(bool, NeedsPCRBinding, (const std::vector<uint8_t>&), (override));

  MOCK_METHOD(int,
              GetWrongAuthAttempts,
              (const std::vector<uint8_t>&),
              (override));

  MOCK_METHOD(bool,
              CheckCredential,
              (const uint64_t,
               const std::vector<std::vector<uint8_t>>&,
               const std::vector<uint8_t>&,
               const brillo::SecureBlob&,
               std::vector<uint8_t>*,
               std::vector<uint8_t>*,
               brillo::SecureBlob*,
               brillo::SecureBlob*,
               LECredBackendError*,
               std::vector<uint8_t>*),
              (override));

  MOCK_METHOD(bool,
              ResetCredential,
              (const uint64_t,
               const std::vector<std::vector<uint8_t>>&,
               const std::vector<uint8_t>&,
               const brillo::SecureBlob&,
               std::vector<uint8_t>*,
               std::vector<uint8_t>*,
               LECredBackendError*,
               std::vector<uint8_t>*),
              (override));

  MOCK_METHOD(bool,
              RemoveCredential,
              (const uint64_t,
               const std::vector<std::vector<uint8_t>>&,
               const std::vector<uint8_t>&,
               std::vector<uint8_t>*),
              (override));

  MOCK_METHOD(bool,
              GetLog,
              (const std::vector<uint8_t>&,
               std::vector<uint8_t>*,
               std::vector<LELogEntry>*),
              (override));

  MOCK_METHOD(bool,
              ReplayLogOperation,
              (const std::vector<uint8_t>&,
               const std::vector<std::vector<uint8_t>>&,
               const std::vector<uint8_t>&,
               std::vector<uint8_t>*,
               std::vector<uint8_t>*),
              (override));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_LE_CREDENTIAL_BACKEND_H_
