// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_CDMA_PROXY_INTERFACE_
#define SHILL_MODEM_CDMA_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

namespace shill {

// These are the methods that a ModemManager.Modem.CDMA proxy must support. The
// interface is provided so that it can be mocked in tests.
class ModemCDMAProxyInterface {
 public:
  virtual ~ModemCDMAProxyInterface() {}

  virtual uint32 Activate(const std::string &carrier) = 0;
  virtual void GetRegistrationState(uint32 *cdma_1x_state,
                                    uint32 *evdo_state) = 0;
  virtual uint32 GetSignalQuality() = 0;
};

// ModemManager.Modem.CDMA signal listener to be associated with the proxy.
class ModemCDMAProxyListener {
 public:
  virtual ~ModemCDMAProxyListener() {}

  virtual void OnCDMAActivationStateChanged(
      uint32 activation_state,
      uint32 activation_error,
      const DBusPropertiesMap &status_changes) = 0;
  virtual void OnCDMARegistrationStateChanged(uint32 state_1x,
                                              uint32 state_evdo) = 0;
  virtual void OnCDMASignalQualityChanged(uint32 strength) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_CDMA_PROXY_INTERFACE_
