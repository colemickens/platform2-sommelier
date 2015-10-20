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
#include <vector>
#include <string>

#include <base/cancelable_callback.h>
#include <base/logging.h>
#include <brillo/any.h>
#if defined(__ANDROID__)
#include <service_constants.h>
#else
#include <chromeos/dbus/service_constants.h>
#endif  // __ANDROID__
#include <shill/dbus-proxies.h>

using ManagerProxy = org::chromium::flimflam::ManagerProxy;
using DeviceProxy = org::chromium::flimflam::DeviceProxy;
using ServiceProxy = org::chromium::flimflam::ServiceProxy;
using ProfileProxy = org::chromium::flimflam::ProfileProxy;

typedef base::Callback<void(const std::string&,
                            const brillo::Any&)> DbusPropertyChangeCallback;

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
  std::vector<std::unique_ptr<DeviceProxy>> GetDeviceProxies();
  std::vector<std::unique_ptr<ServiceProxy>> GetServiceProxies();
  std::vector<std::unique_ptr<ProfileProxy>> GetProfileProxies();
  std::unique_ptr<DeviceProxy> GetMatchingDeviceProxy(
      const brillo::VariantDictionary& expected_properties);
  std::unique_ptr<ServiceProxy> GetMatchingServiceProxy(
      const brillo::VariantDictionary& expected_properties);
  std::unique_ptr<ProfileProxy> GetMatchingProfileProxy(
      const brillo::VariantDictionary& expected_properties);
  bool GetPropertyValueFromDeviceProxy(DeviceProxy* proxy,
                                       const std::string& property_name,
                                       brillo::Any* property_value);
  bool GetPropertyValueFromServiceProxy(ServiceProxy* proxy,
                                        const std::string& property_name,
                                        brillo::Any* property_value);
  bool GetPropertyValueFromProfileProxy(ProfileProxy* proxy,
                                        const std::string& property_name,
                                        brillo::Any* property_value);
  // Optional outparams: |final_value| & |elapsed_time_seconds|. Pass nullptr
  // if not required. The outparams are only valid if return value is |true|.
  bool WaitForDeviceProxyPropertyValueIn(
      const dbus::ObjectPath& object_path,
      const std::string& property_name,
      const std::vector<brillo::Any>& expected_values,
      int timeout_seconds,
      brillo::Any* final_value,
      int* elapsed_time_seconds);
  bool WaitForServiceProxyPropertyValueIn(
      const dbus::ObjectPath& object_path,
      const std::string& property_name,
      const std::vector<brillo::Any>& expected_values,
      int timeout_seconds,
      brillo::Any* final_value,
      int* elapsed_time_seconds);
  bool WaitForProfileProxyPropertyValueIn(
      const dbus::ObjectPath& object_path,
      const std::string& property_name,
      const std::vector<brillo::Any>& expected_values,
      int timeout_seconds,
      brillo::Any* final_value,
      int* elapsed_time_seconds);
  std::unique_ptr<ServiceProxy> GetServiceProxy(
      const brillo::VariantDictionary& expected_properties);
  std::unique_ptr<ProfileProxy> GetActiveProfileProxy();
  std::unique_ptr<ServiceProxy> ConfigureService(
      const brillo::VariantDictionary& config);
  std::unique_ptr<ServiceProxy> ConfigureServiceByGuid(
      const std::string& guid,
      const brillo::VariantDictionary& config);
  bool ConnectService(const dbus::ObjectPath& object_path,
                      int timeout_seconds);
  bool DisconnectService(const dbus::ObjectPath& object_path,
                         int timeout_seconds);
  bool CreateProfile(const std::string& profile_name);
  bool RemoveProfile(const std::string& profile_name);
  bool PushProfile(const std::string& profile_name);
  bool PopProfile(const std::string& profile_name);
  bool PopAnyProfile();

 private:
  bool GetPropertyValueFromManager(const std::string& property_name,
                                   brillo::Any* property_value);
  dbus::ObjectPath GetObjectPathForActiveProfile();
  bool SetLoggingInternal(int level, const std::string& tags);
  template<typename Proxy> std::unique_ptr<Proxy> GetProxyForObjectPath(
      const dbus::ObjectPath& object_path);
  template<typename Proxy> std::vector<std::unique_ptr<Proxy>> GetProxies(
      const std::string& object_paths_property_name);
  template<typename Proxy> std::unique_ptr<Proxy> GetMatchingProxy(
      const std::string& object_paths_property_name,
      const brillo::VariantDictionary& expected_properties);
  template<typename Proxy> bool WaitForProxyPropertyValueIn(
      const dbus::ObjectPath& object_path,
      const std::string& property_name,
      const std::vector<brillo::Any>& expected_values,
      int timeout_seconds,
      brillo::Any* final_value,
      int* elapsed_time_seconds);

  scoped_refptr<dbus::Bus> dbus_bus_;
  ManagerProxy shill_manager_proxy_;
};
#endif //PROXY_DBUS_CLIENT_H
