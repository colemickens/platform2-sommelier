// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/settings_keys.h"

namespace fides {
namespace keys {

const char kFidesPrefix[] = "org.chromium.settings";

const char kSources[] = "sources";

namespace sources {

const char kName[] = "name";
const char kStatus[] = "status";
const char kType[] = "type";
const char kAccess[] = "access";
const char kBlobFormat[] = "blob_format";
const char kNVRamIndex[] = "nvram_index";

}  // namespace sources
}  // namespace keys
}  // namespace fides
