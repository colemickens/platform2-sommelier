// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SUPPLICANT_MOCK_SUPPLICANT_BSS_PROXY_H_
#define SHILL_SUPPLICANT_MOCK_SUPPLICANT_BSS_PROXY_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/supplicant/supplicant_bss_proxy_interface.h"

namespace shill {

class MockSupplicantBSSProxy : public SupplicantBSSProxyInterface {
 public:
  MockSupplicantBSSProxy();
  ~MockSupplicantBSSProxy() override;

  MOCK_METHOD(void, Die, ());  // So we can EXPECT the dtor.

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSupplicantBSSProxy);
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_MOCK_SUPPLICANT_BSS_PROXY_H_
