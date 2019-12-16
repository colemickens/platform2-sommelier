// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/crypto_error.h"

#include <iostream>
#include <type_traits>

namespace cryptohome {

std::ostream& operator<<(std::ostream& os, const CryptoError& obj) {
  os << static_cast<std::underlying_type<CryptoError>::type>(obj);
  return os;
}

}  // namespace cryptohome
