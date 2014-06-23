// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_MOCK_CONFIG_LOADER_H_
#define MIST_MOCK_CONFIG_LOADER_H_

#include <base/compiler_specific.h>
#include <gmock/gmock.h>

#include "mist/config_loader.h"

namespace mist {

class MockConfigLoader : public ConfigLoader {
 public:
  MockConfigLoader();
  virtual ~MockConfigLoader() OVERRIDE;

  MOCK_METHOD0(LoadDefaultConfig, bool());
  MOCK_METHOD1(LoadConfig, bool(const base::FilePath& config_file));
  MOCK_CONST_METHOD2(GetUsbModemInfo,
                     const UsbModemInfo*(uint16 vendor_id, uint16 product_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConfigLoader);
};

}  // namespace mist

#endif  // MIST_MOCK_CONFIG_LOADER_H_
