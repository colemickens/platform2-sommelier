// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/ip_addr.h"

#include <cstring>

namespace peerd {

bool operator==(const ip_addr& v1, const ip_addr& v2) {
    return !memcmp(&v1, &v2, sizeof(v1));
}

}  // namespace peerd
