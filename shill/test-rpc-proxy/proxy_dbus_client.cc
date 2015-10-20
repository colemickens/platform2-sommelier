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

#include <base/message_loop/message_loop.h>

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

template<typename Proxy> bool IsProxyPropertyValueIn(
    Proxy* proxy,
    const std::string& property_name,
    const std::vector<brillo::Any>& expected_values,
    time_t wait_start_time,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  brillo::Any property_value;
  if ((GetPropertyValueFromProxy<Proxy>(proxy, property_name, &property_value)) &&
      (std::find(expected_values.begin(), expected_values.end(),
                 property_value) != expected_values.end())) {
    if (final_value) {
      *final_value = property_value;
    }
    if (elapsed_time_seconds) {
      *elapsed_time_seconds = time(nullptr) - wait_start_time;
    }
    return true;
  }
  return false;
}

// This is invoked when the dbus detects a change in one of
// the properties of the proxy. We need to check if the property
// we're interested in has reached one of the expected values.
void PropertyChangedSignalCallback(
    const std::string& watched_property_name,
    const std::vector<brillo::Any>& expected_values,
    const std::string& changed_property_name,
    const brillo::Any& new_property_value) {
  if ((watched_property_name == changed_property_name) &&
      (std::find(expected_values.begin(), expected_values.end(),
                 new_property_value) != expected_values.end())) {
    // Unblock the waiting function by stopping the message loop.
    base::MessageLoop::current()->QuitNow();
  }
}

// This is invoked to indicate whether dbus successfully connected our
// signal callback or not.
void PropertyChangedOnConnectedCallback(
    const std::string& /* watched_property_name */,
    const std::string& /* interface */,
    const std::string& /* signal_name */,
    bool success) {
  CHECK(success);
}

template<typename Proxy>
void HelpRegisterPropertyChangedSignalHandler(
    Proxy* proxy,
    dbus::ObjectProxy::OnConnectedCallback on_connected_callback,
    const DbusPropertyChangeCallback& signal_callback) {
  // Re-order |on_connected_callback| and |signal_callback|, to meet
  // the requirements of RegisterPropertyChangedSignalHandler().
  proxy->RegisterPropertyChangedSignalHandler(
      signal_callback, on_connected_callback);
}

template<typename EventValueType, typename ConditionChangeCallbackType>
bool WaitForCondition(
    base::Callback<bool(time_t, EventValueType*, int*)>
        condition_termination_checker,
    base::Callback<ConditionChangeCallbackType> condition_change_callback,
    base::Callback<void(const base::Callback<ConditionChangeCallbackType>&)>
        condition_change_callback_registrar,
    int timeout_seconds,
    EventValueType* final_value,
    int* elapsed_seconds) {
  const time_t start_time(time(nullptr));
  const base::TimeDelta timeout(base::TimeDelta::FromSeconds(timeout_seconds));
  base::CancelableClosure wait_timeout_callback;
  base::CancelableCallback<ConditionChangeCallbackType> change_callback;

  if (condition_termination_checker.Run(
          start_time, final_value, elapsed_seconds)) {
    return true;
  }

  wait_timeout_callback.Reset(base::MessageLoop::QuitClosure());
  change_callback.Reset(condition_change_callback);

  condition_change_callback_registrar.Run(change_callback.callback());

  // Add timeout, in case we never hit the expected condition.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      wait_timeout_callback.callback(),
      timeout);

  // Wait for the condition to occur within |timeout_seconds|.
  base::MessageLoop::current()->Run();

  wait_timeout_callback.Cancel();
  change_callback.Cancel();

  // We could have reached here either because we timed out or
  // because we reached the condition.
  return condition_termination_checker.Run(
      start_time, final_value, elapsed_seconds);
}
} // namespace

ProxyDbusClient::ProxyDbusClient(scoped_refptr<dbus::Bus> bus)
  : dbus_bus_(bus),
    shill_manager_proxy_(dbus_bus_) {
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
  SetLoggingInternal(kLogLevel, log_scopes);
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

bool ProxyDbusClient::WaitForDeviceProxyPropertyValueIn(
    const dbus::ObjectPath& object_path,
    const std::string& property_name,
    const std::vector<brillo::Any>& expected_values,
    int timeout_seconds,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  return WaitForProxyPropertyValueIn<DeviceProxy>(
      object_path, property_name, expected_values, timeout_seconds,
      final_value, elapsed_time_seconds);
}

bool ProxyDbusClient::WaitForServiceProxyPropertyValueIn(
    const dbus::ObjectPath& object_path,
    const std::string& property_name,
    const std::vector<brillo::Any>& expected_values,
    int timeout_seconds,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  return WaitForProxyPropertyValueIn<ServiceProxy>(
      object_path, property_name, expected_values, timeout_seconds,
      final_value, elapsed_time_seconds);
}

bool ProxyDbusClient::WaitForProfileProxyPropertyValueIn(
    const dbus::ObjectPath& object_path,
    const std::string& property_name,
    const std::vector<brillo::Any>& expected_values,
    int timeout_seconds,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  return WaitForProxyPropertyValueIn<ProfileProxy>(
      object_path, property_name, expected_values, timeout_seconds,
      final_value, elapsed_time_seconds);
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::GetServiceProxy(
    const brillo::VariantDictionary& expected_properties) {
  dbus::ObjectPath service_path;
  brillo::ErrorPtr error;
  if (!shill_manager_proxy_.GetService(
          expected_properties, &service_path, &error)) {
    return nullptr;
  }
  return std::unique_ptr<ServiceProxy>(
      new ServiceProxy(dbus_bus_, service_path));
}

std::unique_ptr<ProfileProxy> ProxyDbusClient::GetActiveProfileProxy() {
  return GetProxyForObjectPath<ProfileProxy>(GetObjectPathForActiveProfile());
}

std::unique_ptr<ServiceProxy> ProxyDbusClient::ConfigureService(
    const brillo::VariantDictionary& config) {
  dbus::ObjectPath service_path;
  brillo::ErrorPtr error;
  if(!shill_manager_proxy_.ConfigureService(
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
  if(!shill_manager_proxy_.ConfigureService(
      guid_config, &service_path, &error)) {
    return nullptr;
  }
  return GetProxyForObjectPath<ServiceProxy>(service_path);
}

bool ProxyDbusClient::ConnectService(
    const dbus::ObjectPath& object_path,
    int timeout_seconds) {
  auto proxy = GetProxyForObjectPath<ServiceProxy>(object_path);
  brillo::ErrorPtr error;
  if (!proxy->Connect(&error)) {
    return false;
  }
  const std::vector<brillo::Any> expected_values = {
    brillo::Any(std::string(shill::kStatePortal)),
    brillo::Any(std::string(shill::kStateOnline)) };
  return WaitForProxyPropertyValueIn<ServiceProxy>(
      object_path, shill::kStateProperty, expected_values,
      timeout_seconds, nullptr, nullptr);
}

bool ProxyDbusClient::DisconnectService(
    const dbus::ObjectPath& object_path,
    int timeout_seconds) {
  auto proxy = GetProxyForObjectPath<ServiceProxy>(object_path);
  brillo::ErrorPtr error;
  if (!proxy->Disconnect(&error)) {
    return false;
  }
  const std::vector<brillo::Any> expected_values = {
    brillo::Any(std::string(shill::kStateIdle)) };
  return WaitForProxyPropertyValueIn<ServiceProxy>(
      object_path, shill::kStateProperty, expected_values,
      timeout_seconds, nullptr, nullptr);
}

bool ProxyDbusClient::CreateProfile(const std::string& profile_name) {
  dbus::ObjectPath profile_path;
  brillo::ErrorPtr error;
  return shill_manager_proxy_.CreateProfile(
      profile_name, &profile_path, &error);
}

bool ProxyDbusClient::RemoveProfile(const std::string& profile_name) {
  brillo::ErrorPtr error;
  return shill_manager_proxy_.RemoveProfile(profile_name, &error);
}

bool ProxyDbusClient::PushProfile(const std::string& profile_name) {
  dbus::ObjectPath profile_path;
  brillo::ErrorPtr error;
  return shill_manager_proxy_.PushProfile(
      profile_name, &profile_path, &error);
}

bool ProxyDbusClient::PopProfile(const std::string& profile_name) {
  brillo::ErrorPtr error;
  return shill_manager_proxy_.PopProfile(profile_name, &error);
}

bool ProxyDbusClient::PopAnyProfile() {
  brillo::ErrorPtr error;
  return shill_manager_proxy_.PopAnyProfile(&error);
}

bool ProxyDbusClient::GetPropertyValueFromManager(
    const std::string& property_name,
    brillo::Any* property_value) {
  return GetPropertyValueFromProxy(
      &shill_manager_proxy_, property_name, property_value);
}

dbus::ObjectPath ProxyDbusClient::GetObjectPathForActiveProfile() {
  brillo::Any property_value;
  if (!GetPropertyValueFromManager(
        shill::kActiveProfileProperty, &property_value)) {
    return dbus::ObjectPath();
  }
  return dbus::ObjectPath(property_value.Get<std::string>());
}

bool ProxyDbusClient::SetLoggingInternal(int level, const std::string& tags) {
  bool is_success = true;
  brillo::ErrorPtr error;
  is_success &= shill_manager_proxy_.SetDebugLevel(level, &error);
  is_success &= shill_manager_proxy_.SetDebugTags(tags, &error);
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

template<typename Proxy>
bool ProxyDbusClient::WaitForProxyPropertyValueIn(
    const dbus::ObjectPath& object_path,
    const std::string& property_name,
    const std::vector<brillo::Any>& expected_values,
    int timeout_seconds,
    brillo::Any* final_value,
    int* elapsed_time_seconds) {
  base::Callback<bool(time_t, brillo::Any*, int*)>
    condition_termination_checker;
  DbusPropertyChangeCallback condition_change_callback;
  base::Callback<void(const DbusPropertyChangeCallback&)>
    condition_change_callback_registrar;
  // Creates a local proxy using |object_path| instead of accepting the proxy
  // from the caller since we cannot deregister the signal property change
  // callback associated.
  auto proxy = GetProxyForObjectPath<Proxy>(object_path);

  condition_termination_checker =
      base::Bind(&IsProxyPropertyValueIn<Proxy>,
                 proxy.get(),
                 property_name,
                 expected_values);
  condition_change_callback =
      base::Bind(&PropertyChangedSignalCallback,
                 property_name,
                 expected_values);
  condition_change_callback_registrar =
      base::Bind(&HelpRegisterPropertyChangedSignalHandler<Proxy>,
                 base::Unretained(proxy.get()),
                 base::Bind(&PropertyChangedOnConnectedCallback,
                            property_name));
  return WaitForCondition(
      condition_termination_checker, condition_change_callback,
      condition_change_callback_registrar,
      timeout_seconds, final_value, elapsed_time_seconds);
}
