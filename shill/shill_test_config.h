// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHILL_TEST_CONFIG_H_
#define SHILL_SHILL_TEST_CONFIG_H_

#include <string>

#include <base/files/scoped_temp_dir.h>

#include "shill/shill_config.h"

namespace shill {

class TestConfig : public Config {
 public:
  TestConfig();
  ~TestConfig() override;

  std::string GetRunDirectory() override;
  std::string GetStorageDirectory() override;

 private:
  base::ScopedTempDir dir_;

  DISALLOW_COPY_AND_ASSIGN(TestConfig);
};

}  // namespace shill

#endif  // SHILL_SHILL_TEST_CONFIG_H_
