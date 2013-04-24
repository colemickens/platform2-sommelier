// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

namespace login_manager {
class NssUtil;

namespace keygen {

// Generates a keypair using the user's NSSDB, extracts the public half and
// stores it at |filename|.
int GenerateKey(const std::string& filename, NssUtil* nss);

}  // namespace keygen

}  // namespace login_manager
