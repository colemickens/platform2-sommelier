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
#include <time.h>

#include "proxy_dbus_client.h"

const char ProxyDbusClient::kCommonLogScopes[] =
  "connection+dbus+device+link+manager+portal+service";
const int ProxyDbusClient::kLogLevel = -4;

bool ProxyDbusClient::GetManagerProperty(
    const std::string property_name,
    chromeos::Any& property_value) {
  chromeos::VariantDictionary props;
  chromeos::ErrorPtr error;
  if (!shill_manager_proxy_->GetProperties(&props, &error)) {
    return false;
  }
  if (props.find(property_name) == props.end()) {
    return false;
  }
  property_value = props[property_name];
  return true;
}

void ProxyDbusClient::PropertyChangedSignalCallback(
    const std::string& property_name,
    const chromeos::Any& property_value) {
  // TODO:
  if (wait_for_change_property_name_ == property_name) {
  }
}

void ProxyDbusClient::PropertyChangedOnConnectedCallback(
    const std::string& interface,
    const std::string& signal_name,
    bool success) {
  // TODO:
}

bool ProxyDbusClient::ComparePropertyValue(
    chromeos::VariantDictionary& properties,
    const std::string property_name,
    const std::vector<chromeos::Any>& expected_values,
    chromeos::Any& final_value) {
  if (properties.find(property_name) == properties.end()) {
    return false;
  }
  final_value = properties[property_name];
  for (auto const& value : expected_values) {
    if (value == final_value) {
      return true;
    }
  }
  return false;
}

bool ProxyDbusClient::WaitForPropertyValueIn(
    chromeos::VariantDictionary& properties,
    const std::string property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_in_seconds,
    chromeos::Any& final_value,
    int& duration) {
  bool is_success = false;
  time_t start(time(NULL));

  if (ComparePropertyValue(properties, property_name,
                           expected_values, final_value)) {
    is_success = true;
  }

  // TODO: Need some mechanism to wait for timeout_in_seconds time for
  // the property to be set in PropertyChangedSignalCallback.

 end:
  time_t end(time(NULL));
  duration = difftime(start, end);
  return is_success;
}

std::string ProxyDbusClient::GetActiveProfilePath() {
  chromeos::Any property_value;
  if (!GetManagerProperty(shill::kActiveProfileProperty, property_value)) {
    return std::string();
  }
  return property_value.Get<std::string>();
}

bool ProxyDbusClient::SetLogging(int level, std::string& tags) {
  bool is_success = true;
  chromeos::ErrorPtr error;
  is_success &= shill_manager_proxy_->SetDebugLevel(level, &error);
  is_success &= shill_manager_proxy_->SetDebugTags(tags, &error);
  return is_success;
}

void ProxyDbusClient::SetLoggingForTest(Technology tech) {
  std::string log_scopes(kCommonLogScopes);
  switch (tech) {
    case TECHNOLOGY_CELLULAR:
      log_scopes += "+cellular";
      break;
    case TECHNOLOGY_ETHERNET:
      log_scopes += "+ethernet";
      break;
    case TECHNOLOGY_VPN:
      log_scopes += "+vpn";
      break;
    case TECHNOLOGY_WIFI:
      log_scopes += "+wifi";
      break;
    case TECHNOLOGY_WIMAX:
      log_scopes += "+wimax";
      break;
  }
  (void)SetLogging(kLogLevel, log_scopes);
}

bool ProxyDbusClient::WaitForPropertyValueIn(
    ManagerProxyPtr proxy,
    const std::string property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_in_seconds,
    chromeos::Any& final_value,
    int& duration) {
  proxy->RegisterPropertyChangedSignalHandler(
       base::Bind(&ProxyDbusClient::PropertyChangedSignalCallback,
                   weak_ptr_factory_.GetWeakPtr()),
       base::Bind(&ProxyDbusClient::PropertyChangedOnConnectedCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  chromeos::VariantDictionary props;
  chromeos::ErrorPtr error;
  if (!proxy->GetProperties(&props, &error)) {
    return false;
  }
  return WaitForPropertyValueIn(
      props, property_name, expected_values, timeout_in_seconds,
      final_value, duration);
}

bool ProxyDbusClient::WaitForPropertyValueIn(
    DeviceProxyPtr proxy,
    const std::string property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_in_seconds,
    chromeos::Any& final_value,
    int& duration) {
  proxy->RegisterPropertyChangedSignalHandler(
       base::Bind(&ProxyDbusClient::PropertyChangedSignalCallback,
                   weak_ptr_factory_.GetWeakPtr()),
       base::Bind(&ProxyDbusClient::PropertyChangedOnConnectedCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  chromeos::VariantDictionary props;
  chromeos::ErrorPtr error;
  if (!proxy->GetProperties(&props, &error)) {
    return false;
  }
  return WaitForPropertyValueIn(
      props, property_name, expected_values, timeout_in_seconds,
      final_value, duration);
}

bool ProxyDbusClient::WaitForPropertyValueIn(
    ServiceProxyPtr proxy,
    const std::string property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_in_seconds,
    chromeos::Any& final_value,
    int& duration) {
  proxy->RegisterPropertyChangedSignalHandler(
       base::Bind(&ProxyDbusClient::PropertyChangedSignalCallback,
                   weak_ptr_factory_.GetWeakPtr()),
       base::Bind(&ProxyDbusClient::PropertyChangedOnConnectedCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  chromeos::VariantDictionary props;
  chromeos::ErrorPtr error;
  if (!proxy->GetProperties(&props, &error)) {
    return false;
  }
  return WaitForPropertyValueIn(
      props, property_name, expected_values, timeout_in_seconds,
      final_value, duration);
}
bool ProxyDbusClient::WaitForPropertyValueIn(
    ProfileProxyPtr proxy,
    const std::string property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_in_seconds,
    chromeos::Any& final_value,
    int& duration) {
  proxy->RegisterPropertyChangedSignalHandler(
       base::Bind(&ProxyDbusClient::PropertyChangedSignalCallback,
                   weak_ptr_factory_.GetWeakPtr()),
       base::Bind(&ProxyDbusClient::PropertyChangedOnConnectedCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  chromeos::VariantDictionary props;
  chromeos::ErrorPtr error;
  if (!proxy->GetProperties(&props, &error)) {
    return false;
  }
  return WaitForPropertyValueIn(
      props, property_name, expected_values, timeout_in_seconds,
      final_value, duration);
}

DeviceProxyPtr ProxyDbusClient::GetDeviceProxy(std::string device_path) {
  dbus::ObjectPath object_path(device_path);
  return DeviceProxyPtr(
      new org::chromium::flimflam::DeviceProxy(dbus_bus_, object_path));
}

ServiceProxyPtr ProxyDbusClient::GetServiceProxy(std::string service_path) {
  dbus::ObjectPath object_path(service_path);
  return ServiceProxyPtr(
      new org::chromium::flimflam::ServiceProxy(dbus_bus_, object_path));
}

ProfileProxyPtr ProxyDbusClient::GetProfileProxy(std::string profile_path) {
  dbus::ObjectPath object_path(profile_path);
  return ProfileProxyPtr(
      new org::chromium::flimflam::ProfileProxy(dbus_bus_, object_path));
}

std::vector<DeviceProxyPtr> ProxyDbusClient::GetDeviceProxies() {
  chromeos::Any property_value;
  if (!GetManagerProperty(shill::kDevicesProperty, property_value)) {
    return std::vector<DeviceProxyPtr>();
  }
  std::vector<std::string> device_paths =
    property_value.Get<std::vector<std::string>>();
  std::vector<DeviceProxyPtr> proxies;
  for (const std::string& device_path : device_paths) {
    proxies.push_back(GetDeviceProxy(device_path));
  }
  return proxies;
}

std::vector<ServiceProxyPtr> ProxyDbusClient::GetServiceProxies() {
  chromeos::Any property_value;
  if (!GetManagerProperty(shill::kServicesProperty, property_value)) {
    return std::vector<ServiceProxyPtr>();
  }
  std::vector<std::string> service_paths =
    property_value.Get<std::vector<std::string>>();
  std::vector<ServiceProxyPtr> proxies;
  for (const std::string& service_path : service_paths) {
    proxies.push_back(GetServiceProxy(service_path));
  }
  return proxies;
}

std::vector<ProfileProxyPtr> ProxyDbusClient::GetProfileProxies() {
  chromeos::Any property_value;
  if (!GetManagerProperty(shill::kProfilesProperty, property_value)) {
    return std::vector<ProfileProxyPtr>();
  }
  std::vector<std::string> profile_paths =
    property_value.Get<std::vector<std::string>>();
  std::vector<ProfileProxyPtr> proxies;
  for (const std::string& profile_path : profile_paths) {
    proxies.push_back(GetProfileProxy(profile_path));
  }
  return proxies;
}

ProfileProxyPtr ProxyDbusClient::GetActiveProfileProxy() {
  return GetProfileProxy(GetActiveProfilePath());
}

DeviceProxyPtr ProxyDbusClient::GetMatchingDeviceProxy(
    chromeos::VariantDictionary& params) {
  std::vector <DeviceProxyPtr> device_proxies = GetDeviceProxies();
  for (auto device_proxy : device_proxies) {
    chromeos::VariantDictionary props;
    chromeos::ErrorPtr error;
    if (!device_proxy->GetProperties(&props, &error)) {
      continue;
    }
    bool all_params_matched = true;
    for (auto const& param : params) {
      if (props[param.first] != param.second) {
        all_params_matched = false;
        break;
      }
    }
    if (all_params_matched == true) {
      return device_proxy;
    }
  }
  return nullptr;
}

ServiceProxyPtr ProxyDbusClient::GetMatchingServiceProxy(
    chromeos::VariantDictionary& params) {
  dbus::ObjectPath service_path;
  chromeos::ErrorPtr error;
  if (!shill_manager_proxy_->GetService(
      params, &service_path, &error)) {
    return nullptr;
  }
  return ServiceProxyPtr(
      new org::chromium::flimflam::ServiceProxy(dbus_bus_, service_path));
}

ServiceProxyPtr ProxyDbusClient::ConfigureService(
    chromeos::VariantDictionary& config) {
  dbus::ObjectPath service_path;
  chromeos::ErrorPtr error;
  if(!shill_manager_proxy_->ConfigureService(
      config, &service_path, &error)) {
    return nullptr;
  }
  return GetServiceProxy(service_path.value());
}

ServiceProxyPtr ProxyDbusClient::ConfigureServiceByGuid(
    std::string guid,
    chromeos::VariantDictionary& config) {
  dbus::ObjectPath service_path;
  chromeos::ErrorPtr error;
  chromeos::VariantDictionary guid_config(config);
  guid_config[shill::kGuidProperty] = guid;
  if(!shill_manager_proxy_->ConfigureService(
      guid_config, &service_path, &error)) {
    return nullptr;
  }
  return GetServiceProxy(service_path.value());
}

bool ProxyDbusClient::ConnectService(
    ServiceProxyPtr proxy,
    int timeout_in_seconds) {
  chromeos::ErrorPtr error;
  if (!proxy->Connect(&error)) {
    return false;
  }
  int duration;
  chromeos::Any final_value;
  std::vector<chromeos::Any> expected_values;
  expected_values.push_back(chromeos::Any("portal"));
  expected_values.push_back(chromeos::Any("online"));
  return WaitForPropertyValueIn(
      proxy, shill::kStateProperty, expected_values,
      timeout_in_seconds, final_value, duration);
}

bool ProxyDbusClient::DisconnectService(
    ServiceProxyPtr proxy,
    int timeout_in_seconds) {
  chromeos::ErrorPtr error;
  if (!proxy->Disconnect(&error)) {
    return false;
  }
  int duration;
  chromeos::Any final_value;
  std::vector<chromeos::Any> expected_values;
  expected_values.push_back(chromeos::Any("idle"));
  return WaitForPropertyValueIn(
      proxy, shill::kStateProperty, expected_values,
      timeout_in_seconds, final_value, duration);
}

bool ProxyDbusClient::CreateProfile(std::string profile_name) {
  dbus::ObjectPath profile_path;
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->CreateProfile(
      profile_name,
      &profile_path,
      &error);
}

bool ProxyDbusClient::RemoveProfile(std::string profile_name) {
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->RemoveProfile(
      profile_name,
      &error);
}

bool ProxyDbusClient::PushProfile(std::string profile_name) {
  dbus::ObjectPath profile_path;
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->PushProfile(
      profile_name,
      &profile_path,
      &error);
}

bool ProxyDbusClient::PopProfile(std::string profile_name) {
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->PopProfile(
      profile_name,
      &error);
}

bool ProxyDbusClient::PopAnyProfile() {
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->PopAnyProfile(&error);
}
