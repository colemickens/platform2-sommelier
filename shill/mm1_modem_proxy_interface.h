// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_PROXY_INTERFACE_
#define SHILL_MM1_MODEM_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_properties.h"

namespace shill {
class AsyncCallHandler;
class Error;

namespace mm1 {

// These are the methods that a org.freedesktop.ModemManager1.Modem
// proxy must support.  The interface is provided so that it can be
// mocked in tests.  All calls are made asynchronously. Call
// completion is signalled through the corresponding 'OnXXXCallback'
// method in the ProxyDelegate interface.
class ModemProxyInterface {
 public:
  virtual ~ModemProxyInterface() {}

  virtual void Enable(const bool &enable,
                      AsyncCallHandler *call_handler,
                      int timeout) = 0;
  virtual void ListBearers(AsyncCallHandler *call_handler,
                           int timeout) = 0;
  virtual void CreateBearer(
      const DBusPropertiesMap &properties,
      AsyncCallHandler *call_handler,
      int timeout) = 0;
  virtual void DeleteBearer(const ::DBus::Path &bearer,
                            AsyncCallHandler *call_handler,
                            int timeout) = 0;
  virtual void Reset(AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void FactoryReset(const std::string &code,
                            AsyncCallHandler *call_handler,
                            int timeout) = 0;
  virtual void SetAllowedModes(const uint32_t &modes,
                               const uint32_t &preferred,
                               AsyncCallHandler *call_handler,
                               int timeout) = 0;
  virtual void SetBands(const std::vector< uint32_t > &bands,
                        AsyncCallHandler *call_handler,
                        int timeout) = 0;
  virtual void Command(const std::string &cmd,
                       const uint32_t &user_timeout,
                       AsyncCallHandler *call_handler,
                       int timeout) = 0;

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
  virtual const std::string Driver() = 0;
  virtual const std::string Plugin() = 0;
  virtual const std::string EquipmentIdentifier() = 0;
  virtual uint32_t UnlockRequired() = 0;
  virtual const std::map< uint32_t, uint32_t > UnlockRetries() = 0;
  virtual uint32_t State() = 0;
  virtual uint32_t AccessTechnologies() = 0;
  virtual const ::DBus::Struct< uint32_t, bool > SignalQuality() = 0;
  virtual uint32_t SupportedModes() = 0;
  virtual uint32_t AllowedModes() = 0;
  virtual uint32_t PreferredMode() = 0;
  virtual const std::vector< uint32_t > SupportedBands() = 0;
  virtual const std::vector< uint32_t > Bands() = 0;
};


// ModemManager1.Modem signal delegate to be associated with the proxy.
class ModemProxyDelegate {
 public:
  virtual ~ModemProxyDelegate() {}

  // handle signals
  virtual void OnStateChanged(const uint32_t &old,
                              const uint32_t &_new,
                              const uint32_t &reason) = 0;

  // Handle async callbacks
  virtual void OnEnableCallback(const Error &error,
                                AsyncCallHandler *call_handler) = 0;
  virtual void OnListBearersCallback(
      const std::vector< ::DBus::Path > &bearers,
      const Error &error,
      AsyncCallHandler *call_handler) = 0;
  virtual void OnCreateBearerCallback(const ::DBus::Path &bearer,
                                      const Error &error,
                                      AsyncCallHandler *call_handler) = 0;
  virtual void OnDeleteBearerCallback(const Error &error,
                                      AsyncCallHandler *call_handler) = 0;
  virtual void OnResetCallback(const Error &error,
                               AsyncCallHandler *call_handler) = 0;
  virtual void OnFactoryResetCallback(const Error &error,
                                      AsyncCallHandler *call_handler) = 0;
  virtual void OnSetAllowedModesCallback(const Error &error,
                                         AsyncCallHandler *call_handler) = 0;
  virtual void OnSetBandsCallback(const Error &error,
                                  AsyncCallHandler *call_handler) = 0;
  virtual void OnCommandCallback(const std::string &response,
                                 const Error &error,
                                 AsyncCallHandler *call_handler) = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_PROXY_INTERFACE_
