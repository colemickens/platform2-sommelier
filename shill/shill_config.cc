// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_config.h"

namespace shill {

// static
const char Config::kDefaultRunDirectory[] = RUNDIR;
// static
const char Config::kDefaultStorageDirectory[] = "/var/cache/shill";
// static
const char Config::kDefaultUserStorageFormat[] = RUNDIR "/user_profiles/%s";

Config::Config() {}

Config::~Config() {}

std::string Config::GetRunDirectory() {
  return kDefaultRunDirectory;
}

std::string Config::GetStorageDirectory() {
  return kDefaultStorageDirectory;
}

std::string Config::GetUserStorageDirectoryFormat() {
  return kDefaultUserStorageFormat;
}

}  // namespace shill
