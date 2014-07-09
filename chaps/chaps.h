// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "pkcs11/cryptoki.h"

namespace chaps {

extern const char* kChapsServicePath;
extern const char* kChapsServiceName;
extern const size_t kTokenLabelSize;
extern const CK_ATTRIBUTE_TYPE kKeyBlobAttribute;
extern const CK_ATTRIBUTE_TYPE kAuthDataAttribute;
extern const CK_ATTRIBUTE_TYPE kLegacyAttribute;

}  // namespace
