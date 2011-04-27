// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_config.h"

namespace shill {

const char Config::kShillDefaultPrefsDir[] = "/var/lib/shill";

Config::Config(/*FilePath &prefs_dir, FilePath &def_prefs_dir*/)
  /* : prefs_dir_(prefs_dir), def_prefs_dir_(def_prefs_dir) */ {}

}  // namespace shill
