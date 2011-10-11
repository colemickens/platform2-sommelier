// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

namespace chaps {

extern const char* kChapsServicePath;
extern const char* kChapsServiceName;
extern const int kTokenLabelSize;

// A PKCS #11 template is encoded as an attribute-value map for dbus-c++.
typedef std::map<uint32_t, std::vector<uint8_t> > AttributeValueMap;

}  // namespace
