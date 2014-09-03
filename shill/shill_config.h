// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHILL_CONFIG_H_
#define SHILL_SHILL_CONFIG_H_

#include <string>

#include <base/macros.h>

namespace shill {

class Config {
 public:
  Config();
  virtual ~Config();

  virtual std::string GetRunDirectory();
  virtual std::string GetStorageDirectory();
  virtual std::string GetUserStorageDirectory();

 private:
  static const char kDefaultRunDirectory[];
  static const char kDefaultStorageDirectory[];
  static const char kDefaultUserStorageDirectory[];

  DISALLOW_COPY_AND_ASSIGN(Config);
};

}  // namespace shill

#endif  // SHILL_SHILL_CONFIG_H_
