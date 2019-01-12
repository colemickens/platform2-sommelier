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

  MOCK_METHOD0(device_info, DeviceInfo*());
  MOCK_METHOD0(modem_info, ModemInfo*());
  MOCK_METHOD0(ethernet_provider, EthernetProvider*());
#if !defined(DISABLE_WIRED_8021X)
  MOCK_CONST_METHOD0(ethernet_eap_provider, EthernetEapProvider*());
#endif  // DISABLE_WIRED_8021X
  MOCK_METHOD0(wimax_provider, WiMaxProvider*());
  MOCK_METHOD0(mutable_store, PropertyStore*());
  MOCK_CONST_METHOD0(store, const PropertyStore&());
  MOCK_CONST_METHOD0(run_path, const base::FilePath&());
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD3(SetProfileForService, void(const ServiceRefPtr& to_set,
                                          const std::string& profile,
                                          Error* error));
  MOCK_METHOD1(RegisterDevice, void(const DeviceRefPtr& to_manage));
  MOCK_METHOD1(DeregisterDevice, void(const DeviceRefPtr& to_forget));
  MOCK_METHOD1(HasService, bool(const ServiceRefPtr& to_manage));
  MOCK_METHOD1(RegisterService, void(const ServiceRefPtr& to_manage));
  MOCK_METHOD1(UpdateService, void(const ServiceRefPtr& to_update));
  MOCK_METHOD1(DeregisterService, void(const ServiceRefPtr& to_forget));
  MOCK_METHOD1(RegisterDefaultServiceCallback,
               int(const ServiceCallback& callback));
  MOCK_METHOD1(DeregisterDefaultServiceCallback, void(int tag));
  MOCK_METHOD1(UpdateDevice, void(const DeviceRefPtr& to_update));
  MOCK_METHOD0(UpdateWiFiProvider, void());
  MOCK_METHOD1(OnDeviceGeolocationInfoUpdated,
               void(const DeviceRefPtr& device));
  MOCK_METHOD1(RecheckPortalOnService, void(const ServiceRefPtr& service));
  MOCK_METHOD2(HandleProfileEntryDeletion,
               bool(const ProfileRefPtr& profile,
                    const std::string& entry_name));
  MOCK_CONST_METHOD0(GetDefaultService, ServiceRefPtr());
  MOCK_METHOD3(GetServiceWithStorageIdentifier,
               ServiceRefPtr(const ProfileRefPtr& profile,
                             const std::string& entry_name,
                             Error* error));
  MOCK_METHOD3(CreateTemporaryServiceFromProfile,
               ServiceRefPtr(const ProfileRefPtr& profile,
                             const std::string& entry_name,
                             Error* error));
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_METHOD0(UpdateEnabledTechnologies, void());
  MOCK_METHOD1(IsPortalDetectionEnabled, bool(Technology::Identifier tech));
  MOCK_CONST_METHOD1(IsServiceEphemeral,
                     bool(const ServiceConstRefPtr& service));
  MOCK_CONST_METHOD2(IsProfileBefore,
                     bool(const ProfileRefPtr& a,
                          const ProfileRefPtr& b));
  MOCK_CONST_METHOD1(IsTechnologyConnected,
                     bool(Technology::Identifier tech));
  MOCK_CONST_METHOD1(IsTechnologyLinkMonitorEnabled,
                     bool(Technology::Identifier tech));
  MOCK_CONST_METHOD1(IsTechnologyAutoConnectDisabled,
                     bool(Technology::Identifier tech));
  MOCK_METHOD2(RequestScan, void(const std::string& technology, Error* error));
  MOCK_CONST_METHOD0(GetPortalCheckHttpUrl, const std::string&());
  MOCK_CONST_METHOD0(GetPortalCheckHttpsUrl, const std::string&());
  MOCK_CONST_METHOD0(GetPortalCheckFallbackHttpUrls,
                     const std::vector<std::string>&());
  MOCK_CONST_METHOD0(GetPortalCheckInterval, int());
  MOCK_METHOD0(IsSuspending, bool());
  MOCK_CONST_METHOD1(GetEnabledDeviceWithTechnology,
                     DeviceRefPtr(Technology::Identifier technology));
  MOCK_CONST_METHOD1(GetEnabledDeviceByLinkName,
                     DeviceRefPtr(const std::string& link_name));
  MOCK_CONST_METHOD0(GetMinimumMTU, int());
  MOCK_CONST_METHOD0(GetJailVpnClients, bool());
  MOCK_CONST_METHOD1(ShouldAcceptHostnameFrom,
                     bool(const std::string& device_name));
  MOCK_CONST_METHOD1(IsDHCPv6EnabledForDevice,
                     bool(const std::string& device_name));
  MOCK_METHOD1(SetBlacklistedDevices,
               void(const std::vector<std::string>& blacklisted_devices));
  MOCK_METHOD1(SetDHCPv6EnabledDevices,
               void(const std::vector<std::string>& device_list));
  MOCK_METHOD2(SetTechnologyOrder,
               void(const std::string& order, Error* error));
  MOCK_METHOD1(SetIgnoreUnknownEthernet, void(bool ignore));
  MOCK_METHOD1(SetStartupPortalList, void(const std::string& portal_list));
  MOCK_METHOD0(SetPassiveMode, void());
  MOCK_METHOD1(SetPrependDNSServers,
               void(const std::string& prepend_dns_servers));
  MOCK_METHOD1(SetMinimumMTU, void(const int mtu));
  MOCK_METHOD1(SetAcceptHostnameFrom, void(const std::string& hostname_from));
  MOCK_CONST_METHOD0(ignore_unknown_ethernet, bool());
  MOCK_CONST_METHOD1(FilterPrependDNSServersByFamily,
                     std::vector<std::string>(IPAddress::Family family));
  MOCK_CONST_METHOD0(GetSuspendDurationUsecs, int64_t());
  MOCK_METHOD0(OnInnerDevicesChanged, void());
  MOCK_METHOD3(ClaimDevice,
               void(const std::string& claimer_name,
                    const std::string& interface_name, Error* error));
  MOCK_METHOD4(ReleaseDevice, void(const std::string& claimer_name,
                                   const std::string& interface_name,
                                   bool* claimer_removed, Error* error));
  MOCK_METHOD0(OnDeviceClaimerVanished, void());
  MOCK_METHOD0(GetDeviceInterfaceNames, std::vector<std::string>());
  MOCK_CONST_METHOD0(GetFTEnabled, bool());
  MOCK_CONST_METHOD0(ActiveProfile, const ProfileRefPtr&());

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
