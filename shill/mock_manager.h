// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MANAGER_H_
#define SHILL_MOCK_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/manager.h"

namespace shill {

class MockEthernetProvider;

class MockManager : public Manager {
 public:
  MockManager(ControlInterface* control_interface,
              EventDispatcher* dispatcher,
              Metrics* metrics);
  ~MockManager() override;

  MOCK_METHOD(DeviceInfo*, device_info, (), (override));
#if !defined(DISABLE_CELLULAR)
  MOCK_METHOD(ModemInfo*, modem_info, (), (override));
#endif  // DISABLE_CELLULAR
  MOCK_METHOD(EthernetProvider*, ethernet_provider, (), (override));
#if !defined(DISABLE_WIRED_8021X)
  MOCK_METHOD(EthernetEapProvider*,
              ethernet_eap_provider,
              (),
              (const, override));
#endif  // DISABLE_WIRED_8021X
  MOCK_METHOD(const PropertyStore&, store, (), (const, override));
  MOCK_METHOD(const base::FilePath&, run_path, (), (const, override));
  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void,
              SetProfileForService,
              (const ServiceRefPtr&, const std::string&, Error*),
              (override));
  MOCK_METHOD(bool,
              MatchProfileWithService,
              (const ServiceRefPtr&),
              (override));
  MOCK_METHOD(void, RegisterDevice, (const DeviceRefPtr&), (override));
  MOCK_METHOD(void, DeregisterDevice, (const DeviceRefPtr&), (override));
  MOCK_METHOD(bool, HasService, (const ServiceRefPtr&), (override));
  MOCK_METHOD(void, RegisterService, (const ServiceRefPtr&), (override));
  MOCK_METHOD(void, UpdateService, (const ServiceRefPtr&), (override));
  MOCK_METHOD(void, DeregisterService, (const ServiceRefPtr&), (override));
  MOCK_METHOD(void, UpdateDevice, (const DeviceRefPtr&), (override));
#if !defined(DISABLE_WIFI)
  MOCK_METHOD(void, UpdateWiFiProvider, (), (override));
#endif  // DISABLE_WIFI
  MOCK_METHOD(ServiceRefPtr, GetPrimaryPhysicalService, (), (override));
  MOCK_METHOD(void,
              OnDeviceGeolocationInfoUpdated,
              (const DeviceRefPtr&),
              (override));
  MOCK_METHOD(void, RecheckPortalOnService, (const ServiceRefPtr&), (override));
  MOCK_METHOD(bool,
              HandleProfileEntryDeletion,
              (const ProfileRefPtr&, const std::string&),
              (override));
  MOCK_METHOD(ServiceRefPtr, GetDefaultService, (), (const, override));
  MOCK_METHOD(ServiceRefPtr,
              GetServiceWithStorageIdentifier,
              (const ProfileRefPtr&, const std::string&, Error*),
              (override));
  MOCK_METHOD(ServiceRefPtr,
              CreateTemporaryServiceFromProfile,
              (const ProfileRefPtr&, const std::string&, Error*),
              (override));
  MOCK_METHOD(bool, IsConnected, (), (const, override));
  MOCK_METHOD(void, UpdateEnabledTechnologies, (), (override));
  MOCK_METHOD(bool, IsPortalDetectionEnabled, (Technology), (override));
  MOCK_METHOD(bool,
              IsServiceEphemeral,
              (const ServiceConstRefPtr&),
              (const, override));
  MOCK_METHOD(bool,
              IsProfileBefore,
              (const ProfileRefPtr&, const ProfileRefPtr&),
              (const, override));
  MOCK_METHOD(bool, IsTechnologyConnected, (Technology), (const, override));
  MOCK_METHOD(bool,
              IsTechnologyLinkMonitorEnabled,
              (Technology),
              (const, override));
  MOCK_METHOD(bool,
              IsTechnologyAutoConnectDisabled,
              (Technology),
              (const, override));
  MOCK_METHOD(void, RequestScan, (const std::string&, Error*), (override));
  MOCK_METHOD(const std::string&, GetPortalCheckHttpUrl, (), (const, override));
  MOCK_METHOD(const std::string&,
              GetPortalCheckHttpsUrl,
              (),
              (const, override));
  MOCK_METHOD(const std::vector<std::string>&,
              GetPortalCheckFallbackHttpUrls,
              (),
              (const, override));
  MOCK_METHOD(bool, IsSuspending, (), (override));
  MOCK_METHOD(DeviceRefPtr,
              GetEnabledDeviceWithTechnology,
              (Technology),
              (const, override));
  MOCK_METHOD(DeviceRefPtr,
              GetEnabledDeviceByLinkName,
              (const std::string&),
              (const, override));
  MOCK_METHOD(int, GetMinimumMTU, (), (const, override));
  MOCK_METHOD(bool, GetJailVpnClients, (), (const, override));
  MOCK_METHOD(bool,
              ShouldAcceptHostnameFrom,
              (const std::string&),
              (const, override));
  MOCK_METHOD(bool,
              IsDHCPv6EnabledForDevice,
              (const std::string&),
              (const, override));
  MOCK_METHOD(void,
              SetBlacklistedDevices,
              (const std::vector<std::string>&),
              (override));
  MOCK_METHOD(void,
              SetDHCPv6EnabledDevices,
              (const std::vector<std::string>&),
              (override));
  MOCK_METHOD(void,
              SetTechnologyOrder,
              (const std::string&, Error*),
              (override));
  MOCK_METHOD(void, SetIgnoreUnknownEthernet, (bool), (override));
  MOCK_METHOD(void, SetStartupPortalList, (const std::string&), (override));
  MOCK_METHOD(void, SetPassiveMode, (), (override));
  MOCK_METHOD(void, SetPrependDNSServers, (const std::string&), (override));
  MOCK_METHOD(void, SetMinimumMTU, (const int), (override));
  MOCK_METHOD(void, SetAcceptHostnameFrom, (const std::string&), (override));
  MOCK_METHOD(bool, ignore_unknown_ethernet, (), (const, override));
  MOCK_METHOD(std::vector<std::string>,
              FilterPrependDNSServersByFamily,
              (IPAddress::Family),
              (const, override));
  MOCK_METHOD(int64_t, GetSuspendDurationUsecs, (), (const, override));
  MOCK_METHOD(void, OnInnerDevicesChanged, (), (override));
  MOCK_METHOD(void,
              ClaimDevice,
              (const std::string&, const std::string&, Error*),
              (override));
  MOCK_METHOD(void,
              ReleaseDevice,
              (const std::string&, const std::string&, bool*, Error*),
              (override));
  MOCK_METHOD(void, OnDeviceClaimerVanished, (), (override));
  MOCK_METHOD(std::vector<std::string>,
              GetDeviceInterfaceNames,
              (),
              (override));
  MOCK_METHOD(const ProfileRefPtr&, ActiveProfile, (), (const, override));
  MOCK_METHOD(ServiceRefPtr, GetFirstEthernetService, (), (override));

  // Getter and setter for a mocked device info instance.
  DeviceInfo* mock_device_info() { return mock_device_info_; }
  void set_mock_device_info(DeviceInfo* mock_device_info) {
    mock_device_info_ = mock_device_info;
  }

 private:
  DeviceInfo* mock_device_info_;
  std::unique_ptr<MockEthernetProvider> mock_ethernet_provider_;

  DISALLOW_COPY_AND_ASSIGN(MockManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_MANAGER_H_
