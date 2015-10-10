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


namespace {
template<typename Proxy> bool GetPropertyValueFromProxy(
    Proxy* proxy,
    const std::string& property_name,
    brillo::Any* property_value) {
  CHECK(property_value);
  brillo::VariantDictionary proxy_properties;
  brillo::ErrorPtr error;
  CHECK(proxy->GetProperties(&proxy_properties, &error));
  if (proxy_properties.find(property_name) == proxy_properties.end()) {
    return false;
  }
  *property_value = proxy_properties[property_name];
  return true;
}
}

ProxyDbusClient::ProxyDbusClient(
    scoped_refptr<dbus::Bus> bus)
  : dbus_bus_(bus),
    weak_ptr_factory_(this),
    wait_for_change_property_name_(""),
    shill_manager_proxy_(new ManagerProxy(dbus_bus_)) {
}

void ProxyDbusClient::PropertyChangedSignalCallback(
    const std::string& property_name,
    const brillo::Any& property_value) {
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
    const brillo::VariantDictionary& properties,
    const std::string& property_name,
    const std::vector<brillo::Any>& expected_values,
    brillo::Any* final_value) {
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
    const brillo::VariantDictionary& properties,
    const std::string& property_name,
    const std::vector<brillo::Any>& expected_values,
    const int timeout_seconds,
    brillo::Any* final_value,
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
    const std::vector<brillo::Any>& expected_values,
    const int timeout_seconds,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  proxy->RegisterPropertyChangedSignalHandler(
       base::Bind(&ProxyDbusClient::PropertyChangedSignalCallback,
                   weak_ptr_factory_.GetWeakPtr()),
       base::Bind(&ProxyDbusClient::PropertyChangedOnConnectedCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  brillo::VariantDictionary props;
  brillo::ErrorPtr error;
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
    const std::vector<brillo::Any>& expected_values,
    const int timeout_seconds,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  proxy->RegisterPropertyChangedSignalHandler(
       base::Bind(&ProxyDbusClient::PropertyChangedSignalCallback,
                   weak_ptr_factory_.GetWeakPtr()),
       base::Bind(&ProxyDbusClient::PropertyChangedOnConnectedCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  brillo::VariantDictionary props;
  brillo::ErrorPtr error;
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
    const std::vector<brillo::Any>& expected_values,
    const int timeout_seconds,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  proxy->RegisterPropertyChangedSignalHandler(
       base::Bind(&ProxyDbusClient::PropertyChangedSignalCallback,
                   weak_ptr_factory_.GetWeakPtr()),
       base::Bind(&ProxyDbusClient::PropertyChangedOnConnectedCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  brillo::VariantDictionary props;
  brillo::ErrorPtr error;
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
    const std::vector<brillo::Any>& expected_values,
    const int timeout_seconds,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  proxy->RegisterPropertyChangedSignalHandler(
       base::Bind(&ProxyDbusClient::PropertyChangedSignalCallback,
                   weak_ptr_factory_.GetWeakPtr()),
       base::Bind(&ProxyDbusClient::PropertyChangedOnConnectedCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  brillo::VariantDictionary props;
  brillo::ErrorPtr error;
  if (!proxy->GetProperties(&props, &error)) {
    return false;
  }
  return WaitForPropertyValueIn(
      props, property_name, expected_values, timeout_seconds,
      final_value, elapsed_time_seconds);
}

std::vector<std::unique_ptr<DeviceProxy>> ProxyDbusClient::GetDeviceProxies() {
  return GetProxies<DeviceProxy>(shill::kDevicesProperty);
}

std::vector<std::unique_ptr<ServiceProxy>> ProxyDbusClient::GetServiceProxies() {
  return GetProxies<ServiceProxy>(shill::kServicesProperty);
}

std::vector<std::unique_ptr<ProfileProxy>> ProxyDbusClient::GetProfileProxies() {
  return GetProxies<ProfileProxy>(shill::kProfilesProperty);
}

std::unique_ptr<DeviceProxy> ProxyDbusClient::GetMatchingDeviceProxy(
    const brillo::VariantDictionary& expected_properties) {
  return GetMatchingProxy<DeviceProxy>(shill::kDevicesProperty, expected_properties);
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::GetMatchingServiceProxy(
    const brillo::VariantDictionary& expected_properties) {
  return GetMatchingProxy<ServiceProxy>(shill::kServicesProperty, expected_properties);
}

std::unique_ptr<ProfileProxy> ProxyDbusClient::GetMatchingProfileProxy(
    const brillo::VariantDictionary& expected_properties) {
  return GetMatchingProxy<ProfileProxy>(shill::kProfilesProperty, expected_properties);
}

bool ProxyDbusClient::GetPropertyValueFromDeviceProxy(
    DeviceProxy* proxy,
    const std::string& property_name,
    brillo::Any* property_value) {
  return GetPropertyValueFromProxy<DeviceProxy>(
      proxy, property_name, property_value);
}

bool ProxyDbusClient::GetPropertyValueFromServiceProxy(
    ServiceProxy* proxy,
    const std::string& property_name,
    brillo::Any* property_value) {
  return GetPropertyValueFromProxy<ServiceProxy>(
      proxy, property_name, property_value);
}

bool ProxyDbusClient::GetPropertyValueFromProfileProxy(
    ProfileProxy* proxy,
    const std::string& property_name,
    brillo::Any* property_value) {
  return GetPropertyValueFromProxy<ProfileProxy>(
      proxy, property_name, property_value);
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::GetServiceProxy(
    const brillo::VariantDictionary& expected_properties) {
  dbus::ObjectPath service_path;
  brillo::ErrorPtr error;
  if (!shill_manager_proxy_->GetService(
          expected_properties, &service_path, &error)) {
    return nullptr;
  }
  return std::unique_ptr<ServiceProxy>(new ServiceProxy(dbus_bus_, service_path));
}

std::unique_ptr<ProfileProxy> ProxyDbusClient::GetActiveProfileProxy() {
  return GetProxyForObjectPath<ProfileProxy>(GetObjectPathForActiveProfile());
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::ConfigureService(
    const brillo::VariantDictionary& config) {
  dbus::ObjectPath service_path;
  brillo::ErrorPtr error;
  if(!shill_manager_proxy_->ConfigureService(
      config, &service_path, &error)) {
    return nullptr;
  }
  return GetProxyForObjectPath<ServiceProxy>(service_path);
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::ConfigureServiceByGuid(
    const std::string& guid,
    const brillo::VariantDictionary& config) {
  dbus::ObjectPath service_path;
  brillo::ErrorPtr error;
  brillo::VariantDictionary guid_config(config);
  guid_config[shill::kGuidProperty] = guid;
  if(!shill_manager_proxy_->ConfigureService(
      guid_config, &service_path, &error)) {
    return nullptr;
  }
  return GetProxyForObjectPath<ServiceProxy>(service_path);
}

bool ProxyDbusClient::ConnectService(
    ServiceProxy* proxy,
    int timeout_seconds) {
  brillo::ErrorPtr error;
  if (!proxy->Connect(&error)) {
    return false;
  }
  int elapsed_time_seconds;
  brillo::Any final_value;
  const std::vector<brillo::Any> expected_values = {
    brillo::Any(std::string(shill::kStatePortal)),
    brillo::Any(std::string(shill::kStateOnline)) };
  return WaitForPropertyValueIn(
      proxy, shill::kStateProperty, expected_values,
      timeout_seconds, &final_value, &elapsed_time_seconds);
}

bool ProxyDbusClient::DisconnectService(
    ServiceProxy* proxy,
    int timeout_seconds) {
  brillo::ErrorPtr error;
  if (!proxy->Disconnect(&error)) {
    return false;
  }
  int elapsed_time_seconds;
  brillo::Any final_value;
  const std::vector<brillo::Any> expected_values = {
    brillo::Any(std::string(shill::kStateIdle)) };
  return WaitForPropertyValueIn(
      proxy, shill::kStateProperty, expected_values,
      timeout_seconds, &final_value, &elapsed_time_seconds);
}

bool ProxyDbusClient::CreateProfile(const std::string& profile_name) {
  dbus::ObjectPath profile_path;
  brillo::ErrorPtr error;
  return shill_manager_proxy_->CreateProfile(
      profile_name, &profile_path, &error);
}

bool ProxyDbusClient::RemoveProfile(const std::string& profile_name) {
  brillo::ErrorPtr error;
  return shill_manager_proxy_->RemoveProfile(profile_name, &error);
}

bool ProxyDbusClient::PushProfile(const std::string& profile_name) {
  dbus::ObjectPath profile_path;
  brillo::ErrorPtr error;
  return shill_manager_proxy_->PushProfile(
      profile_name, &profile_path, &error);
}

bool ProxyDbusClient::PopProfile(const std::string& profile_name) {
  brillo::ErrorPtr error;
  return shill_manager_proxy_->PopProfile(profile_name, &error);
}

bool ProxyDbusClient::PopAnyProfile() {
  brillo::ErrorPtr error;
  return shill_manager_proxy_->PopAnyProfile(&error);
}

bool ProxyDbusClient::GetPropertyValueFromManager(
    const std::string& property_name,
    brillo::Any* property_value) {
  return GetPropertyValueFromProxy(
      shill_manager_proxy_.get(), property_name, property_value);
}

dbus::ObjectPath ProxyDbusClient::GetObjectPathForActiveProfile() {
  brillo::Any property_value;
  if (!GetPropertyValueFromManager(
        shill::kActiveProfileProperty, &property_value)) {
    return dbus::ObjectPath();
  }
  return dbus::ObjectPath(property_value.Get<std::string>());
}

bool ProxyDbusClient::SetLogging(int level, const std::string& tags) {
  bool is_success = true;
  brillo::ErrorPtr error;
  is_success &= shill_manager_proxy_->SetDebugLevel(level, &error);
  is_success &= shill_manager_proxy_->SetDebugTags(tags, &error);
  return is_success;
}

template<typename Proxy>
std::unique_ptr<Proxy> ProxyDbusClient::GetProxyForObjectPath(
    const dbus::ObjectPath& object_path) {
  return std::unique_ptr<Proxy>(new Proxy(dbus_bus_, object_path));
}

// Templated functions to return the object path property_name based on
template<typename Proxy>
std::vector<std::unique_ptr<Proxy>> ProxyDbusClient::GetProxies(
    const std::string& object_paths_property_name) {
  brillo::Any object_paths;
  if (!GetPropertyValueFromManager(object_paths_property_name, &object_paths)) {
    return std::vector<std::unique_ptr<Proxy>>();
  }
  std::vector<std::unique_ptr<Proxy>> proxies;
  for (const auto& object_path :
       object_paths.Get<std::vector<dbus::ObjectPath>>()) {
    proxies.emplace_back(GetProxyForObjectPath<Proxy>(object_path));
  }
  return proxies;
}

template<typename Proxy>
std::unique_ptr<Proxy> ProxyDbusClient::GetMatchingProxy(
    const std::string& object_paths_property_name,
    const brillo::VariantDictionary& expected_properties) {
  for (auto& proxy : GetProxies<Proxy>(object_paths_property_name)) {
    brillo::VariantDictionary proxy_properties;
    brillo::ErrorPtr error;
    CHECK(proxy->GetProperties(&proxy_properties, &error));
    bool all_expected_properties_matched = true;
    for (const auto& expected_property : expected_properties) {
      if (proxy_properties[expected_property.first] != expected_property.second) {
        all_expected_properties_matched = false;
        break;
      }
    }
    if (all_expected_properties_matched) {
      return std::move(proxy);
    }
  }
  return nullptr;
}
