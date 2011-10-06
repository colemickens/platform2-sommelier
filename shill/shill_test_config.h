// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_TEST_CONFIG_
#define SHILL_TEST_CONFIG_

#include <string>

#include <base/memory/scoped_temp_dir.h>

#include "shill/shill_config.h"

namespace shill {

class TestConfig : public Config {
 public:
  TestConfig();
  virtual ~TestConfig();

  virtual std::string GetRunDirectory();
  virtual std::string GetStorageDirectory();

 private:
  ScopedTempDir dir_;

  DISALLOW_COPY_AND_ASSIGN(TestConfig);
};

}  // namespace shill

#endif  // SHILL_TEST_CONFIG_
