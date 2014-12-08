// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/constants.h"

namespace privetd {

const char kDefaultStateFilePath[] = "/var/lib/privetd/privetd.state";

namespace errors {

const char kPrivetdErrorDomain[] = "privetd";
const char kInvalidClientCommitment[] = "invalid_client_commitment";

}  // namespace errors

}  // namespace privetd
