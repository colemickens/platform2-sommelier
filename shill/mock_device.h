// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DEVICE_H_
#define SHILL_MOCK_DEVICE_H_

#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/device.h"
#include "shill/geolocation_info.h"

namespace shill {

class MockDevice : public Device {
 public:
  MockDevice(Manager* manager,
             const std::string& link_name,
             const std::string& address,
             int interface_index);
  ~MockDevice() override;

  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD2(Start, void(Error* error,
                           const EnabledStateChangedCallback& callback));
  MOCK_METHOD2(Stop, void(Error* error,
                          const EnabledStateChangedCallback& callback));
  MOCK_METHOD1(SetEnabled, void(bool));
  MOCK_METHOD3(SetEnabledPersistent, void(bool enable,
                                          Error* error,
                                          const ResultCallback& callback));
  MOCK_METHOD3(SetEnabledNonPersistent, void(bool enable,
                                             Error* error,
                                             const ResultCallback& callback));
  MOCK_METHOD2(Scan, void(Error* error, const std::string& reason));
  MOCK_METHOD1(Load, bool(StoreInterface* storage));
  MOCK_METHOD1(Save, bool(StoreInterface* storage));
  MOCK_METHOD0(DisableIPv6, void());
  MOCK_METHOD0(EnableIPv6, void());
  MOCK_METHOD0(EnableIPv6Privacy, void());
  MOCK_METHOD1(SetLooseRouting, void(bool));
  MOCK_METHOD1(SetIsMultiHomed, void(bool is_multi_homed));
  MOCK_METHOD0(RestartPortalDetection, bool());
  MOCK_METHOD0(RequestPortalDetection, bool());
  MOCK_METHOD0(GetReceiveByteCount, uint64_t());
  MOCK_METHOD0(GetTransmitByteCount, uint64_t());
  MOCK_CONST_METHOD1(IsConnectedToService, bool(const ServiceRefPtr& service));
  MOCK_CONST_METHOD0(technology, Technology::Identifier());
  MOCK_METHOD1(OnBeforeSuspend, void(const ResultCallback& callback));
  MOCK_METHOD1(OnDarkResume, void(const ResultCallback& callback));
  MOCK_METHOD0(OnAfterResume, void());
  MOCK_METHOD0(OnConnectionUpdated, void());
  MOCK_METHOD0(OnIPv6AddressChanged, void());
  MOCK_CONST_METHOD0(GetGeolocationObjects, std::vector<GeolocationInfo>());
  MOCK_METHOD0(OnIPv6DnsServerAddressesChanged, void());
  MOCK_METHOD0(StartConnectivityTest, bool());
  MOCK_CONST_METHOD0(connection, const ConnectionRefPtr&());
  MOCK_METHOD0(RefreshIPConfig, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDevice);
};

}  // namespace shill

#endif  // SHILL_MOCK_DEVICE_H_
