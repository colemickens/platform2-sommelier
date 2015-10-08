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

ProxyDbusClient::ProxyDbusClient(
    scoped_refptr<dbus::Bus> bus)
  : dbus_bus_(bus),
    weak_ptr_factory_(this),
    wait_for_change_property_name_(""),
    shill_manager_proxy_(new ManagerProxy(dbus_bus_)) {
}

bool ProxyDbusClient::GetManagerProperty(
    const std::string& property_name,
    chromeos::Any* property_value) {
  chromeos::VariantDictionary props;
  chromeos::ErrorPtr error;
  if (!shill_manager_proxy_->GetProperties(&props, &error)) {
    return false;
  }
  if (props.find(property_name) == props.end()) {
    return false;
  }
  *property_value = props[property_name];
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
    const chromeos::VariantDictionary& properties,
    const std::string& property_name,
    const std::vector<chromeos::Any>& expected_values,
    chromeos::Any* final_value) {
  auto it = properties.find(property_name);
  if (it == properties.end()) {
    return false;
  }
  *final_value = it->second;
  for (const auto& value : expected_values) {
    if (value == *final_value) {
      return true;
    }
  }
  return false;
}

bool ProxyDbusClient::WaitForPropertyValueIn(
    const chromeos::VariantDictionary& properties,
    const std::string& property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_seconds,
    chromeos::Any* final_value,
    int* elapsed_time_seconds) {
  bool is_success = false;
  time_t start(time(NULL));

  if (ComparePropertyValue(properties, property_name,
                           expected_values, final_value)) {
    is_success = true;
  }

  // TODO: Need some mechanism to wait for timeout_seconds time for
  // the property to be set in PropertyChangedSignalCallback.

 end:
  time_t end(time(NULL));
  *elapsed_time_seconds = end - start;
  return is_success;
}

std::string ProxyDbusClient::GetActiveProfilePath() {
  chromeos::Any property_value;
  if (!GetManagerProperty(shill::kActiveProfileProperty, &property_value)) {
    return std::string();
  }
  return property_value.Get<std::string>();
}

bool ProxyDbusClient::SetLogging(int level, const std::string& tags) {
  bool is_success = true;
  chromeos::ErrorPtr error;
  is_success &= shill_manager_proxy_->SetDebugLevel(level, &error);
  is_success &= shill_manager_proxy_->SetDebugTags(tags, &error);
  return is_success;
}

void ProxyDbusClient::SetLogging(Technology tech) {
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
    ManagerProxy* proxy,
    const std::string& property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_seconds,
    chromeos::Any* final_value,
    int* elapsed_time_seconds) {
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
      props, property_name, expected_values, timeout_seconds,
      final_value, elapsed_time_seconds);
}

bool ProxyDbusClient::WaitForPropertyValueIn(
    DeviceProxy* proxy,
    const std::string& property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_seconds,
    chromeos::Any* final_value,
    int* elapsed_time_seconds) {
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
      props, property_name, expected_values, timeout_seconds,
      final_value, elapsed_time_seconds);
}

bool ProxyDbusClient::WaitForPropertyValueIn(
    ServiceProxy* proxy,
    const std::string& property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_seconds,
    chromeos::Any* final_value,
    int* elapsed_time_seconds) {
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
      props, property_name, expected_values, timeout_seconds,
      final_value, elapsed_time_seconds);
}
bool ProxyDbusClient::WaitForPropertyValueIn(
    ProfileProxy* proxy,
    const std::string& property_name,
    const std::vector<chromeos::Any>& expected_values,
    const int timeout_seconds,
    chromeos::Any* final_value,
    int* elapsed_time_seconds) {
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
      props, property_name, expected_values, timeout_seconds,
      final_value, elapsed_time_seconds);
}

std::unique_ptr<DeviceProxy> ProxyDbusClient::GetDeviceProxy(
    const std::string& device_path) {
  dbus::ObjectPath object_path(device_path);
  return std::unique_ptr<DeviceProxy>(new DeviceProxy(dbus_bus_, object_path));
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::GetServiceProxy(
    const std::string& service_path) {
  dbus::ObjectPath object_path(service_path);
  return std::unique_ptr<ServiceProxy>(new ServiceProxy(dbus_bus_, object_path));
}

std::unique_ptr<ProfileProxy> ProxyDbusClient::GetProfileProxy(
    const std::string& profile_path) {
  dbus::ObjectPath object_path(profile_path);
  return std::unique_ptr<ProfileProxy>(new ProfileProxy(dbus_bus_, object_path));
}

std::vector<std::unique_ptr<DeviceProxy>> ProxyDbusClient::GetDeviceProxies() {
  chromeos::Any property_value;
  if (!GetManagerProperty(shill::kDevicesProperty, &property_value)) {
    return std::vector<std::unique_ptr<DeviceProxy>>();
  }
  std::vector<std::string> device_paths =
    property_value.Get<std::vector<std::string>>();
  std::vector<std::unique_ptr<DeviceProxy>> proxies;
  for (const std::string& device_path : device_paths) {
    proxies.emplace_back(GetDeviceProxy(device_path));
  }
  return proxies;
}

std::vector<std::unique_ptr<ServiceProxy>> ProxyDbusClient::GetServiceProxies() {
  chromeos::Any property_value;
  if (!GetManagerProperty(shill::kServicesProperty, &property_value)) {
    return std::vector<std::unique_ptr<ServiceProxy>>();
  }
  std::vector<std::string> service_paths =
    property_value.Get<std::vector<std::string>>();
  std::vector<std::unique_ptr<ServiceProxy>> proxies;
  for (const std::string& service_path : service_paths) {
    proxies.emplace_back(GetServiceProxy(service_path));
  }
  return proxies;
}

std::vector<std::unique_ptr<ProfileProxy>> ProxyDbusClient::GetProfileProxies() {
  chromeos::Any property_value;
  if (!GetManagerProperty(shill::kProfilesProperty, &property_value)) {
    return std::vector<std::unique_ptr<ProfileProxy>>();
  }
  std::vector<std::string> profile_paths =
    property_value.Get<std::vector<std::string>>();
  std::vector<std::unique_ptr<ProfileProxy>> proxies;
  for (const std::string& profile_path : profile_paths) {
    proxies.emplace_back(GetProfileProxy(profile_path));
  }
  return proxies;
}

std::unique_ptr<ProfileProxy> ProxyDbusClient::GetActiveProfileProxy() {
  return GetProfileProxy(GetActiveProfilePath());
}

std::unique_ptr<DeviceProxy> ProxyDbusClient::GetMatchingDeviceProxy(
    const chromeos::VariantDictionary& params) {
  for (auto& device_proxy : GetDeviceProxies()) {
    chromeos::VariantDictionary props;
    chromeos::ErrorPtr error;
    if (!device_proxy->GetProperties(&props, &error)) {
      continue;
    }
    bool all_params_matched = true;
    for (const auto& param : params) {
      if (props[param.first] != param.second) {
        all_params_matched = false;
        break;
      }
    }
    if (all_params_matched) {
      return std::move(device_proxy);
    }
  }
  return nullptr;
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::GetMatchingServiceProxy(
    const chromeos::VariantDictionary& params) {
  dbus::ObjectPath service_path;
  chromeos::ErrorPtr error;
  if (!shill_manager_proxy_->GetService(
      params, &service_path, &error)) {
    return nullptr;
  }
  return std::unique_ptr<ServiceProxy>(new ServiceProxy(dbus_bus_, service_path));
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::ConfigureService(
    const chromeos::VariantDictionary& config) {
  dbus::ObjectPath service_path;
  chromeos::ErrorPtr error;
  if(!shill_manager_proxy_->ConfigureService(
      config, &service_path, &error)) {
    return nullptr;
  }
  return GetServiceProxy(service_path.value());
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::ConfigureServiceByGuid(
    const std::string& guid,
    const chromeos::VariantDictionary& config) {
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
    ServiceProxy* proxy,
    int timeout_seconds) {
  chromeos::ErrorPtr error;
  if (!proxy->Connect(&error)) {
    return false;
  }
  int elapsed_time_seconds;
  chromeos::Any final_value;
  const std::vector<chromeos::Any> expected_values = {
    chromeos::Any(std::string(shill::kStatePortal)),
    chromeos::Any(std::string(shill::kStateOnline)) };
  return WaitForPropertyValueIn(
      proxy, shill::kStateProperty, expected_values,
      timeout_seconds, &final_value, &elapsed_time_seconds);
}

bool ProxyDbusClient::DisconnectService(
    ServiceProxy* proxy,
    int timeout_seconds) {
  chromeos::ErrorPtr error;
  if (!proxy->Disconnect(&error)) {
    return false;
  }
  int elapsed_time_seconds;
  chromeos::Any final_value;
  const std::vector<chromeos::Any> expected_values = {
    chromeos::Any(std::string(shill::kStateIdle)) };
  return WaitForPropertyValueIn(
      proxy, shill::kStateProperty, expected_values,
      timeout_seconds, &final_value, &elapsed_time_seconds);
}

bool ProxyDbusClient::CreateProfile(const std::string& profile_name) {
  dbus::ObjectPath profile_path;
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->CreateProfile(
      profile_name, &profile_path, &error);
}

bool ProxyDbusClient::RemoveProfile(const std::string& profile_name) {
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->RemoveProfile(profile_name, &error);
}

bool ProxyDbusClient::PushProfile(const std::string& profile_name) {
  dbus::ObjectPath profile_path;
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->PushProfile(
      profile_name, &profile_path, &error);
}

bool ProxyDbusClient::PopProfile(const std::string& profile_name) {
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->PopProfile(profile_name, &error);
}

bool ProxyDbusClient::PopAnyProfile() {
  chromeos::ErrorPtr error;
  return shill_manager_proxy_->PopAnyProfile(&error);
}
