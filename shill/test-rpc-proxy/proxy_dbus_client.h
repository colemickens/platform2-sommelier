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
#include <brillo/any.h>
#if defined(__ANDROID__)
#include <dbus/service_constants.h>
#else
#include <chromeos/dbus/service_constants.h>
#endif  // __ANDROID__
#include <shill/dbus-proxies.h>

using ManagerProxy = org::chromium::flimflam::ManagerProxy;
using DeviceProxy = org::chromium::flimflam::DeviceProxy;
using ServiceProxy = org::chromium::flimflam::ServiceProxy;
using ProfileProxy = org::chromium::flimflam::ProfileProxy;

class ProxyDbusClient {
 public:
  enum Technology {
    TECHNOLOGY_CELLULAR,
    TECHNOLOGY_ETHERNET,
    TECHNOLOGY_VPN,
    TECHNOLOGY_WIFI,
    TECHNOLOGY_WIMAX
  };
  static const char kCommonLogScopes[];
  static const int kLogLevel;

  ProxyDbusClient(scoped_refptr<dbus::Bus> bus);
  void SetLogging(Technology tech);
  bool WaitForPropertyValueIn(ManagerProxy* proxy,
                              const std::string& property_name,
                              const std::vector<brillo::Any>& expected_values,
                              const int timeout_seconds,
                              brillo::Any* final_value,
                              int* elapsed_time_seconds);
  bool WaitForPropertyValueIn(DeviceProxy* proxy,
                              const std::string& property_name,
                              const std::vector<brillo::Any>& expected_values,
                              const int timeout_seconds,
                              brillo::Any* final_value,
                              int* elapsed_time_seconds);
  bool WaitForPropertyValueIn(ServiceProxy* proxy,
                              const std::string& property_name,
                              const std::vector<brillo::Any>& expected_values,
                              const int timeout_seconds,
                              brillo::Any* final_value,
                              int* elapsed_time_seconds);
  bool WaitForPropertyValueIn(ProfileProxy* proxy,
                              const std::string& property_name,
                              const std::vector<brillo::Any>& expected_values,
                              const int timeout_seconds,
                              brillo::Any* final_value,
                              int* elapsed_time_seconds);
  std::unique_ptr<DeviceProxy> GetDeviceProxy(const std::string& device_path);
  std::unique_ptr<ServiceProxy> GetServiceProxy(const std::string& service_path);
  std::unique_ptr<ProfileProxy> GetProfileProxy(const std::string& profile_path);
  std::vector<std::unique_ptr<DeviceProxy>> GetDeviceProxies();
  std::vector<std::unique_ptr<ServiceProxy>> GetServiceProxies();
  std::vector<std::unique_ptr<ProfileProxy>> GetProfileProxies();
  std::unique_ptr<ProfileProxy> GetActiveProfileProxy();
  std::unique_ptr<DeviceProxy> GetMatchingDeviceProxy(
      const brillo::VariantDictionary& params);
  std::unique_ptr<ServiceProxy> GetMatchingServiceProxy(
      const brillo::VariantDictionary& params);
  std::unique_ptr<ServiceProxy> ConfigureService(
      const brillo::VariantDictionary& config);
  std::unique_ptr<ServiceProxy> ConfigureServiceByGuid(
      const std::string& guid,
      const brillo::VariantDictionary& config);
  bool ConnectService(ServiceProxy* proxy, int timeout_seconds);
  bool DisconnectService(ServiceProxy* proxy, int timeout_seconds);
  bool CreateProfile(const std::string& profile_name);
  bool RemoveProfile(const std::string& profile_name);
  bool PushProfile(const std::string& profile_name);
  bool PopProfile(const std::string& profile_name);
  bool PopAnyProfile();

 private:
  bool GetManagerProperty(const std::string& property_name,
                          brillo::Any* property_value);
  void PropertyChangedSignalCallback(const std::string& property_name,
                                     const brillo::Any& property_value);
  void PropertyChangedOnConnectedCallback(const std::string& interface,
                                          const std::string& signal_name,
                                          bool success);
  bool ComparePropertyValue(const brillo::VariantDictionary& properties,
                            const std::string& property_name,
                            const std::vector<brillo::Any>& expected_values,
                            brillo::Any* final_value);
  bool WaitForPropertyValueIn(const brillo::VariantDictionary& properties,
                              const std::string& property_name,
                              const std::vector<brillo::Any>& expected_values,
                              const int timeout_seconds,
                              brillo::Any* final_value,
                              int* elapsed_time_seconds);
  std::string GetActiveProfilePath();
  bool SetLogging(int level, const std::string& tags);

  scoped_refptr<dbus::Bus> dbus_bus_;
  base::WeakPtrFactory<ProxyDbusClient> weak_ptr_factory_;
  std::string wait_for_change_property_name_;
  std::unique_ptr<ManagerProxy> shill_manager_proxy_;
};
#endif //PROXY_DBUS_CLIENT_H
