// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MODEM_CDMA_PROXY_INTERFACE_H_
#define SHILL_CELLULAR_MODEM_CDMA_PROXY_INTERFACE_H_

#include <string>

#include "shill/callbacks.h"

namespace shill {

class Error;

typedef base::Callback<void(uint32_t)> SignalQualitySignalCallback;
typedef base::Callback<void(uint32_t, uint32_t)>
    RegistrationStateSignalCallback;

typedef base::Callback<void(uint32_t, const Error&)> ActivationResultCallback;
typedef base::Callback<void(uint32_t, const Error&)> SignalQualityCallback;
typedef base::Callback<void(uint32_t, uint32_t,
                            const Error&)> RegistrationStateCallback;

// These are the methods that a ModemManager.Modem.CDMA proxy must support.
// The interface is provided so that it can be mocked in tests.
// All calls are made asynchronously. Call completion is signalled via
// the callbacks passed to the methods.
class ModemCDMAProxyInterface {
 public:
  virtual ~ModemCDMAProxyInterface() {}

  virtual void Activate(const std::string& carrier, Error* error,
                        const ActivationResultCallback& callback,
                        int timeout) = 0;
  virtual void GetRegistrationState(Error* error,
                                    const RegistrationStateCallback& callback,
                                    int timeout) = 0;
  virtual void GetSignalQuality(Error* error,
                                const SignalQualityCallback& callback,
                                int timeout) = 0;

  // Properties.
  virtual const std::string MEID() = 0;

  virtual void set_activation_state_callback(
      const ActivationStateSignalCallback& callback) = 0;
  virtual void set_signal_quality_callback(
      const SignalQualitySignalCallback& callback) = 0;
  virtual void set_registration_state_callback(
      const RegistrationStateSignalCallback& callback) = 0;
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_CDMA_PROXY_INTERFACE_H_
