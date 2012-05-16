// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_WIMAX_PROVIDER_H_
#define SHILL_MOCK_WIMAX_PROVIDER_H_

#include <gmock/gmock.h>

#include "shill/wimax_provider.h"

namespace shill {

class MockWiMaxProvider : public WiMaxProvider {
 public:
  MockWiMaxProvider();
  virtual ~MockWiMaxProvider();

  MOCK_METHOD1(OnDeviceInfoAvailable, void(const std::string &link_name));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiMaxProvider);
};

}  // namespace shill

#endif  // SHILL_MOCK_WIMAX_PROVIDER_H_
