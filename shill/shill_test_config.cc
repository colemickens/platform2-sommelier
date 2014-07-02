// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_test_config.h"

#include <string>

#include "shill/logging.h"

using std::string;

namespace shill {

TestConfig::TestConfig() {
  CHECK(dir_.CreateUniqueTempDir());
}

TestConfig::~TestConfig() {
}

string TestConfig::GetRunDirectory() {
  return dir_.path().value();
}

string TestConfig::GetStorageDirectory() {
  return dir_.path().value();
}

}  // namespace shill
