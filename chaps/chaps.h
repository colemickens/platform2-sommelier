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

constexpr size_t kTokenLabelSize = 32;
constexpr CK_ATTRIBUTE_TYPE kKeyBlobAttribute = CKA_VENDOR_DEFINED + 1;
constexpr CK_ATTRIBUTE_TYPE kAuthDataAttribute = CKA_VENDOR_DEFINED + 2;
constexpr CK_ATTRIBUTE_TYPE kLegacyAttribute = CKA_VENDOR_DEFINED + 3;
// If this attribute is set to true at creation or generation time, then the
// object will not be stored/wrapped in TPM, and will remain purely in software.
constexpr CK_ATTRIBUTE_TYPE kForceSoftwareAttribute = CKA_VENDOR_DEFINED + 4;
// This attribute is set to false if the key is stored in TPM, and true
// otherwise.
constexpr CK_ATTRIBUTE_TYPE kKeyInSoftware = CKA_VENDOR_DEFINED + 5;

}  // namespace chaps

#endif  // CHAPS_CHAPS_H_
