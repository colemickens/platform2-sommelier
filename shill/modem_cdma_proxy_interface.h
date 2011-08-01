// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_CDMA_PROXY_INTERFACE_
#define SHILL_MODEM_CDMA_PROXY_INTERFACE_

#include <base/basictypes.h>

namespace shill {

// These are the methods that a ModemManager.Modem.CDMA proxy must support. The
// interface is provided so that it can be mocked in tests.
class ModemCDMAProxyInterface {
 public:
  virtual ~ModemCDMAProxyInterface() {}

  virtual void GetRegistrationState(uint32 *cdma_1x_state,
                                    uint32 *evdo_state) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_CDMA_PROXY_INTERFACE_
