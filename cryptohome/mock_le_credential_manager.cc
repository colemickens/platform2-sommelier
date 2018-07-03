// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_le_credential_manager.h"

namespace cryptohome {

MockLECredentialManager::MockLECredentialManager(
    LECredentialBackend* le_backend,
    const base::FilePath& le_basedir) :
    LECredentialManager(le_backend, le_basedir) {}
MockLECredentialManager::~MockLECredentialManager() {}

}  // namespace cryptohome
