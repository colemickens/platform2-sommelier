// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_MOCK_WIMAX_PROVIDER_H_
#define SHILL_WIMAX_MOCK_WIMAX_PROVIDER_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/wimax/wimax_provider.h"

namespace shill {

class MockWiMaxProvider : public WiMaxProvider {
 public:
  MockWiMaxProvider();
  ~MockWiMaxProvider() override;

  MOCK_METHOD1(OnDeviceInfoAvailable, void(const std::string& link_name));
  MOCK_METHOD0(OnNetworksChanged, void());
  MOCK_METHOD1(OnServiceUnloaded, bool(const WiMaxServiceRefPtr& service));
  MOCK_METHOD1(SelectCarrier,
               WiMaxRefPtr(const WiMaxServiceConstRefPtr& service));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiMaxProvider);
};

}  // namespace shill

#endif  // SHILL_WIMAX_MOCK_WIMAX_PROVIDER_H_
