// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_config.h"

namespace shill {

// static
const char Config::kDefaultRunDirectory[] = RUNDIR;
// static
const char Config::kDefaultStorageDirectory[] = "/var/cache/shill";
// static
const char Config::kDefaultUserStorageDirectory[] = RUNDIR "/user_profiles/";

Config::Config() {}

Config::~Config() {}

std::string Config::GetRunDirectory() {
  return kDefaultRunDirectory;
}

std::string Config::GetStorageDirectory() {
  return kDefaultStorageDirectory;
}

std::string Config::GetUserStorageDirectory() {
  return kDefaultUserStorageDirectory;
}

}  // namespace shill
