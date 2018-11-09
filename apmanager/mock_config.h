// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_CONFIG_H_
#define APMANAGER_MOCK_CONFIG_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "apmanager/config.h"

namespace apmanager {

class MockConfig : public Config {
 public:
  explicit MockConfig(Manager* manager);
  ~MockConfig() override;

  MOCK_METHOD2(GenerateConfigFile,
               bool(Error* error, std::string* config_str));
  MOCK_METHOD0(ClaimDevice, bool());
  MOCK_METHOD0(ReleaseDevice, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConfig);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_CONFIG_H_
