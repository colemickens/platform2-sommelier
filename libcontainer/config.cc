// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcontainer/config.h"

#include <base/logging.h>

namespace libcontainer {

Config::Config()
    : config_(container_config_create()) {
  // container_config_create() allocates using std::nothrow, so we need to
  // explicitly call abort(2) when allocation fails.
  CHECK(config_);
}

Config::~Config() {
  container_config_destroy(config_);
}

}  // namespace libcontainer

