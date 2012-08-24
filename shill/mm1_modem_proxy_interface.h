// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_PROXY_INTERFACE_
#define SHILL_MM1_MODEM_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

#include "shill/callbacks.h"

namespace shill {
class Error;

namespace mm1 {

typedef base::Callback<void(int32,
                            int32, uint32)> ModemStateChangedSignalCallback;

// These are the methods that a org.freedesktop.ModemManager1.Modem
// proxy must support. The interface is provided so that it can be
// mocked in tests. All calls are made asynchronously. Call completion
// is signalled via the callbacks passed to the methods.
class ModemProxyInterface {
 public:
  virtual ~ModemProxyInterface() {}

  virtual void Enable(bool enable,
                      Error *error,
                      const ResultCallback &callback,
                      int timeout) = 0;
  virtual void ListBearers(Error *error,
                           const DBusPathsCallback &callback,
                           int timeout) = 0;
  virtual void CreateBearer(const DBusPropertiesMap &properties,
                            Error *error,
                            const DBusPathCallback &callback,
                            int timeout) = 0;
  virtual void DeleteBearer(const ::DBus::Path &bearer,
                            Error *error,
                            const ResultCallback &callback,
                            int timeout) = 0;
  virtual void Reset(Error *error,
                     const ResultCallback &callback,
                     int timeout) = 0;
  virtual void FactoryReset(const std::string &code,
                            Error *error,
                            const ResultCallback &callback,
                            int timeout) = 0;
  virtual void SetAllowedModes(const uint32_t &modes,
                               const uint32_t &preferred,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout) = 0;
  virtual void SetBands(const std::vector< uint32_t > &bands,
                        Error *error,
                        const ResultCallback &callback,
                        int timeout) = 0;
  virtual void Command(const std::string &cmd,
                       const uint32_t &user_timeout,
                       Error *error,
                       const StringCallback &callback,
                       int timeout) = 0;


  virtual void set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback) = 0;

  // Properties.
  virtual const ::DBus::Path Sim() = 0;
  virtual uint32_t ModemCapabilities() = 0;
  virtual uint32_t CurrentCapabilities() = 0;
  virtual uint32_t MaxBearers() = 0;
  virtual uint32_t MaxActiveBearers() = 0;
  virtual const std::string Manufacturer() = 0;
  virtual const std::string Model() = 0;
  virtual const std::string Revision() = 0;
  virtual const std::string DeviceIdentifier() = 0;
  virtual const std::string Device() = 0;
  virtual const std::string Plugin() = 0;
  virtual const std::string EquipmentIdentifier() = 0;
  virtual uint32_t UnlockRequired() = 0;
  virtual const std::map< uint32_t, uint32_t > UnlockRetries() = 0;
  virtual uint32_t State() = 0;
  virtual uint32_t AccessTechnologies() = 0;
  virtual const ::DBus::Struct< uint32_t, bool > SignalQuality() = 0;
  virtual const std::vector< std::string > OwnNumbers() = 0;
  virtual uint32_t SupportedModes() = 0;
  virtual uint32_t AllowedModes() = 0;
  virtual uint32_t PreferredMode() = 0;
  virtual const std::vector< uint32_t > SupportedBands() = 0;
  virtual const std::vector< uint32_t > Bands() = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_PROXY_INTERFACE_
