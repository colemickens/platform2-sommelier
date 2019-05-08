// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_H_
#define CHAPS_CHAPS_H_

#include <map>
#include <vector>

#include "pkcs11/cryptoki.h"

// Chaps-specific return values:
#define CKR_CHAPS_SPECIFIC_FIRST (CKR_VENDOR_DEFINED + 0x47474c00)
// Error code returned in case if the operation would block waiting
// for private objects to load for the token.
#define CKR_WOULD_BLOCK_FOR_PRIVATE_OBJECTS (CKR_CHAPS_SPECIFIC_FIRST + 0)

namespace chaps {

constexpr char kSystemTokenPath[] = "/var/lib/chaps";

extern const size_t kTokenLabelSize;
extern const CK_ATTRIBUTE_TYPE kKeyBlobAttribute;
extern const CK_ATTRIBUTE_TYPE kAuthDataAttribute;
extern const CK_ATTRIBUTE_TYPE kLegacyAttribute;

}  // namespace chaps

#endif  // CHAPS_CHAPS_H_
