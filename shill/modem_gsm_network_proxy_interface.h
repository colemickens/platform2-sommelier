// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GSM_NETWORK_PROXY_INTERFACE_
#define SHILL_MODEM_GSM_NETWORK_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

namespace shill {

// These are the methods that a ModemManager.Modem.Gsm.Network proxy must
// support. The interface is provided so that it can be mocked in tests.
class ModemGSMNetworkProxyInterface {
 public:
  virtual ~ModemGSMNetworkProxyInterface() {}

  virtual void Register(const std::string &network_id) = 0;
};

// ModemManager.Modem.Gsm.Network signal listener to be associated with the
// proxy.
class ModemGSMNetworkProxyListener {
 public:
  virtual ~ModemGSMNetworkProxyListener() {}

  virtual void OnGSMNetworkModeChanged(uint32 mode) = 0;
  virtual void OnGSMRegistrationInfoChanged(
      uint32 status,
      const std::string &operator_code,
      const std::string &operator_name) = 0;
  virtual void OnGSMSignalQualityChanged(uint32 quality) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_NETWORK_PROXY_INTERFACE_
