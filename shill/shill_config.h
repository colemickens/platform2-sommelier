// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONFIG_
#define SHILL_CONFIG_

#include <string>

namespace shill {

class Config {
 public:
  static const char kShillDefaultPrefsDir[];

  Config();
  virtual ~Config();

  void UseFlimflamStorageDirs() { use_flimflam_ = true; }

  virtual std::string GetRunDirectory();
  virtual std::string GetStorageDirectory();
  virtual std::string GetUserStorageDirectoryFormat();

 private:
  static const char kDefaultRunDirectory[];
  static const char kDefaultStorageDirectory[];
  static const char kDefaultUserStorageFormat[];
  static const char kFlimflamStorageDirectory[];
  static const char kFlimflamUserStorageFormat[];

  bool use_flimflam_;
};

}  // namespace shill

#endif  // SHILL_CONFIG_
