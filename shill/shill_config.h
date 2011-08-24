// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONFIG_
#define SHILL_CONFIG_

namespace shill {

class Config {
 public:
  Config(/* FilePath &prefs_dir, FilePath &def_prefs_dir */);
  static const char kShillDefaultPrefsDir[];

 private:
  /*
  FilePath prefs_dir_;
  FilePath def_prefs_dir_;
  */
};

}  // namespace shill

#endif  // SHILL_MANAGER_
