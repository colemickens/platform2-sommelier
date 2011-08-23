// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_config.h"

namespace shill {

// static
const char Config::kShillDefaultPrefsDir[] = "/var/lib/shill";

// static
const char Config::kDefaultRunDirectory[] = "/var/run/shill";
// static
const char Config::kDefaultStorageDirectory[] = "/var/cache/shill";
// static
const char Config::kDefaultUserStorageFormat[] = "/home/%s/user/shill";
// static
const char Config::kFlimflamStorageDirectory[] = "/var/cache/flimflam";
// static
const char Config::kFlimflamUserStorageFormat[] = "/home/%s/user/flimflam";

Config::Config() : use_flimflam_(false) {
}

Config::~Config() {}

std::string Config::RunDirectory() {
  return kDefaultRunDirectory;
}

std::string Config::StorageDirectory() {
  return (use_flimflam_ ?
          kFlimflamStorageDirectory :
          kDefaultStorageDirectory);
}

std::string Config::UserStorageDirectoryFormat() {
  return (use_flimflam_ ?
          kFlimflamUserStorageFormat :
          kDefaultUserStorageFormat);
}

}  // namespace shill
