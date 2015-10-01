//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef PROXY_DBUS_CLIENT_H
#define PROXY_DBUS_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/any.h>
#if defined(__ANDROID__)
#include <dbus/service_constants.h>
#else
#include <chromeos/dbus/service_constants.h>
#endif  // __ANDROID__
#include <shill/dbus-proxies.h>

typedef std::shared_ptr<org::chromium::flimflam::ManagerProxy> ManagerProxyPtr;
typedef std::shared_ptr<org::chromium::flimflam::DeviceProxy> DeviceProxyPtr;
typedef std::shared_ptr<org::chromium::flimflam::ServiceProxy> ServiceProxyPtr;
typedef std::shared_ptr<org::chromium::flimflam::ProfileProxy> ProfileProxyPtr;

class ProxyDbusClient {
 public:
  ProxyDbusClient(scoped_refptr<dbus::Bus> bus) :
    dbus_bus_(bus),
    weak_ptr_factory_(this),
    wait_for_change_property_name_("") {
    shill_manager_proxy_.reset(
        new org::chromium::flimflam::ManagerProxy(dbus_bus_));
  }

  enum Technology {
    TECHNOLOGY_CELLULAR,
    TECHNOLOGY_ETHERNET,
    TECHNOLOGY_VPN,
    TECHNOLOGY_WIFI,
    TECHNOLOGY_WIMAX
  };

  static const char kCommonLogScopes[];
  static const int kLogLevel;

  void SetLoggingForTest(Technology tech);
  bool WaitForPropertyValueIn(ManagerProxyPtr proxy,
                              const std::string property_name,
                              const std::vector<chromeos::Any>& expected_values,
                              const int timeout_seconds,
                              chromeos::Any& final_value,
                              int& time);
  bool WaitForPropertyValueIn(DeviceProxyPtr proxy,
                              const std::string property_name,
                              const std::vector<chromeos::Any>& expected_values,
                              const int timeout_seconds,
                              chromeos::Any& final_value,
                              int& time);
  bool WaitForPropertyValueIn(ServiceProxyPtr proxy,
                              const std::string property_name,
                              const std::vector<chromeos::Any>& expected_values,
                              const int timeout_seconds,
                              chromeos::Any& final_value,
                              int& time);
  bool WaitForPropertyValueIn(ProfileProxyPtr proxy,
                              const std::string property_name,
                              const std::vector<chromeos::Any>& expected_values,
                              const int timeout_seconds,
                              chromeos::Any& final_value,
                              int& time);
  DeviceProxyPtr GetDeviceProxy(std::string device_path);
  ServiceProxyPtr GetServiceProxy(std::string service_path);
  ProfileProxyPtr GetProfileProxy(std::string profile_path);
  std::vector<DeviceProxyPtr> GetDeviceProxies();
  std::vector<ServiceProxyPtr> GetServiceProxies();
  std::vector<ProfileProxyPtr> GetProfileProxies();
  ProfileProxyPtr GetActiveProfileProxy();
  DeviceProxyPtr GetMatchingDeviceProxy(chromeos::VariantDictionary& params);
  ServiceProxyPtr GetMatchingServiceProxy(chromeos::VariantDictionary& params);
  ServiceProxyPtr ConfigureService(chromeos::VariantDictionary& config);
  ServiceProxyPtr ConfigureServiceByGuid(std::string guid,
                                         chromeos::VariantDictionary& config);
  bool ConnectService(ServiceProxyPtr proxy, int timeout_seconds);
  bool DisconnectService(ServiceProxyPtr proxy, int timeout_seconds);
  bool CreateProfile(std::string profile_name);
  bool RemoveProfile(std::string profile_name);
  bool PushProfile(std::string profile_name);
  bool PopProfile(std::string profile_name);
  bool PopAnyProfile();

 private:
  scoped_refptr<dbus::Bus> dbus_bus_;
  base::WeakPtrFactory<ProxyDbusClient> weak_ptr_factory_;
  std::string wait_for_change_property_name_;
  ManagerProxyPtr shill_manager_proxy_;

  bool GetManagerProperty(const std::string property_name,
                          chromeos::Any& property_value);
  void PropertyChangedSignalCallback(const std::string& property_name,
                                     const chromeos::Any& property_value);
  void PropertyChangedOnConnectedCallback(const std::string& interface,
                                          const std::string& signal_name,
                                          bool success);
  bool ComparePropertyValue(chromeos::VariantDictionary& properties,
                            const std::string property_name,
                            const std::vector<chromeos::Any>& expected_values,
                            chromeos::Any& final_value);
  bool WaitForPropertyValueIn(chromeos::VariantDictionary& properties,
                              const std::string property_name,
                              const std::vector<chromeos::Any>& expected_values,
                              const int timeout_seconds,
                              chromeos::Any& final_value,
                              int& time);
  std::string GetActiveProfilePath();

  bool SetLogging(int level, std::string& tags);
};
#endif //PROXY_DBUS_CLIENT_H
