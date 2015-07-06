// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/states/error_codes.h"

namespace buffet {
namespace errors {
namespace state {

const char kDomain[] = "buffet_state";

const char kPackageNameMissing[] = "package_name_missing";
const char kPropertyNameMissing[] = "property_name_missing";
const char kPropertyNotDefined[] = "property_not_defined";
const char kPropertyRedefinition[] = "property_redefinition";

}  // namespace state
}  // namespace errors
}  // namespace buffet
