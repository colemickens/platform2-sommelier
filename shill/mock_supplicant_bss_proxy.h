// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_SUPPLICANT_BSS_PROXY_H_
#define MOCK_SUPPLICANT_BSS_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/supplicant_bss_proxy_interface.h"

namespace shill {

class MockSupplicantBSSProxy : public SupplicantBSSProxyInterface {
 public:
  explicit MockSupplicantBSSProxy();
  virtual ~MockSupplicantBSSProxy();

  MOCK_METHOD0(Die, void());  // So we can EXPECT the dtor.

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSupplicantBSSProxy);
};

}  // namespace shill

#endif  // MOCK_SUPPLICANT_BSS_PROXY_H_
