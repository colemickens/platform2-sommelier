// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_IP_ADDRESS_STORE_H_
#define SHILL_MOCK_IP_ADDRESS_STORE_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/ip_address_store.h"

namespace shill {

class MockIPAddressStore : public IPAddressStore {
 public:
  MockIPAddressStore();
  ~MockIPAddressStore() override;

  MOCK_METHOD1(AddUnique, void(const IPAddress& ip));
  MOCK_METHOD0(Clear, void());
  MOCK_CONST_METHOD0(Count, size_t());
  MOCK_CONST_METHOD0(Empty, bool());
  MOCK_METHOD0(GetRandomIP, IPAddress());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIPAddressStore);
};

}  // namespace shill

#endif  // SHILL_MOCK_IP_ADDRESS_STORE_H_
