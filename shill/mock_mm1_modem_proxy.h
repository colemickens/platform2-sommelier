// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MOCK_MODEM_PROXY_H_
#define SHILL_MM1_MOCK_MODEM_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/mm1_modem_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockModemProxy : public ModemProxyInterface {
 public:
  MockModemProxy();
  virtual ~MockModemProxy();

  // Inherited methods from ModemProxyInterface.
  MOCK_METHOD4(Enable, void(bool enable,
                            Error *error,
                            const ResultCallback &callback,
                            int timeout));
  MOCK_METHOD3(ListBearers, void(Error *error,
                                 const DBusPathsCallback &callback,
                                 int timeout));
  MOCK_METHOD4(CreateBearer, void(const DBusPropertiesMap &properties,
                                  Error *error,
                                  const DBusPathCallback &callback,
                                  int timeout));
  MOCK_METHOD4(DeleteBearer, void(const ::DBus::Path &bearer,
                                  Error *error,
                                  const ResultCallback &callback,
                                  int timeout));
  MOCK_METHOD3(Reset, void(Error *error,
                           const ResultCallback &callback,
                           int timeout));
  MOCK_METHOD4(FactoryReset, void(const std::string &code,
                                  Error *error,
                                  const ResultCallback &callback,
                                  int timeout));
  MOCK_METHOD4(SetCurrentCapabilities, void(const uint32_t &capabilities,
                                            Error *error,
                                            const ResultCallback &callback,
                                            int timeout));
  MOCK_METHOD4(SetCurrentModes,
               void(const ::DBus::Struct<uint32_t, uint32_t> &modes,
                    Error *error,
                    const ResultCallback &callback,
                    int timeout));
  MOCK_METHOD4(SetCurrentBands, void(const std::vector<uint32_t> &bands,
                                     Error *error,
                                     const ResultCallback &callback,
                                     int timeout));
  MOCK_METHOD5(Command, void(const std::string &cmd,
                             const uint32_t &user_timeout,
                             Error *error,
                             const StringCallback &callback,
                             int timeout));
  MOCK_METHOD4(SetPowerState, void(const uint32_t &power_state,
                                   Error *error,
                                   const ResultCallback &callback,
                                   int timeout));
  MOCK_METHOD1(set_state_changed_callback, void(
      const ModemStateChangedSignalCallback &callback));

  // Inherited properties from ModemProxyInterface.
  MOCK_METHOD0(Sim, const ::DBus::Path());
  MOCK_METHOD0(SupportedCapabilities, const std::vector<uint32_t>());
  MOCK_METHOD0(CurrentCapabilities, uint32_t());
  MOCK_METHOD0(MaxBearers, uint32_t());
  MOCK_METHOD0(MaxActiveBearers, uint32_t());
  MOCK_METHOD0(Manufacturer, const std::string());
  MOCK_METHOD0(Model, const std::string());
  MOCK_METHOD0(Revision, const std::string());
  MOCK_METHOD0(DeviceIdentifier, const std::string());
  MOCK_METHOD0(Device, const std::string());
  MOCK_METHOD0(Drivers, const std::vector<std::string>());
  MOCK_METHOD0(Plugin, const std::string());
  MOCK_METHOD0(EquipmentIdentifier, const std::string());
  MOCK_METHOD0(UnlockRequired, uint32_t());
  typedef std::map<uint32_t, uint32_t> RetryData;
  MOCK_METHOD0(UnlockRetries, const RetryData());
  MOCK_METHOD1(State, uint32_t(Error *error));
  MOCK_METHOD0(AccessTechnologies, uint32_t());
  typedef ::DBus::Struct<uint32_t, bool> SignalQualityData;
  MOCK_METHOD0(SignalQuality, const SignalQualityData());
  MOCK_METHOD0(OwnNumbers, const std::vector<std::string>());
  typedef std::vector<::DBus::Struct<uint32_t, uint32_t>> SupportedModesData;
  MOCK_METHOD0(SupportedModes, const SupportedModesData());
  typedef ::DBus::Struct<uint32_t, uint32_t> CurrentModesData;
  MOCK_METHOD0(CurrentModes, const CurrentModesData());
  MOCK_METHOD0(SupportedBands, const std::vector<uint32_t>());
  MOCK_METHOD0(CurrentBands, const std::vector<uint32_t>());
  MOCK_METHOD0(SupportedIpFamilies, uint32_t());
  MOCK_METHOD0(PowerState, uint32_t());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MOCK_MODEM_PROXY_H_
