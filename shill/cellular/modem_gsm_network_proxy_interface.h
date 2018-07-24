// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MODEM_GSM_NETWORK_PROXY_INTERFACE_H_
#define SHILL_CELLULAR_MODEM_GSM_NETWORK_PROXY_INTERFACE_H_

#include <map>
#include <string>
#include <vector>

#include "shill/callbacks.h"

namespace shill {

class Error;

typedef std::map<std::string, std::string> GSMScanResult;
typedef std::vector<GSMScanResult> GSMScanResults;

typedef base::Callback<void(uint32_t)> SignalQualitySignalCallback;
typedef base::Callback<void(
    uint32_t,
    const std::string&,
    const std::string&)> RegistrationInfoSignalCallback;
typedef base::Callback<void(uint32_t)> NetworkModeSignalCallback;

typedef base::Callback<void(uint32_t, const Error&)> SignalQualityCallback;
typedef base::Callback<void(uint32_t,
                            const std::string&,
                            const std::string&,
                            const Error&)> RegistrationInfoCallback;
typedef base::Callback<void(const GSMScanResults&,
                            const Error&)> ScanResultsCallback;

// These are the methods that a ModemManager.Modem.Gsm.Network proxy must
// support. The interface is provided so that it can be mocked in tests.
// All calls are made asynchronously.
// XXX fixup comment to reflect new reality
class ModemGSMNetworkProxyInterface {
 public:
  virtual ~ModemGSMNetworkProxyInterface() {}

  virtual void GetRegistrationInfo(Error* error,
                                   const RegistrationInfoCallback& callback,
                                   int timeout) = 0;
  virtual void GetSignalQuality(Error* error,
                                const SignalQualityCallback& callback,
                                int timeout) = 0;
  virtual void Register(const std::string& network_id,
                        Error* error, const ResultCallback& callback,
                        int timeout) = 0;
  virtual void Scan(Error* error, const ScanResultsCallback& callback,
                    int timeout) = 0;

  // Properties.
  virtual uint32_t AccessTechnology() = 0;
  // Signal callbacks
  virtual void set_signal_quality_callback(
      const SignalQualitySignalCallback& callback) = 0;
  virtual void set_network_mode_callback(
      const NetworkModeSignalCallback& callback) = 0;
  virtual void set_registration_info_callback(
      const RegistrationInfoSignalCallback& callback) = 0;
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_GSM_NETWORK_PROXY_INTERFACE_H_
