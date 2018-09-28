// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/shill_client.h"

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <brillo/variant_dictionary.h>
#include <chromeos/dbus/service_constants.h>

namespace vm_tools {
namespace concierge {

ShillClient::ShillClient(scoped_refptr<dbus::Bus> bus)
    : bus_(bus),
      manager_proxy_(new org::chromium::flimflam::ManagerProxy(bus_)) {
  manager_proxy_->RegisterPropertyChangedSignalHandler(
      base::Bind(&ShillClient::OnManagerPropertyChange,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ShillClient::OnManagerPropertyChangeRegistration,
                 weak_factory_.GetWeakPtr()));
}

bool ShillClient::GetResolvConfig(std::vector<std::string>* nameservers,
                                  std::vector<std::string>* search_domains) {
  DCHECK(nameservers);
  DCHECK(search_domains);

  brillo::VariantDictionary manager_props;

  if (!manager_proxy_->GetProperties(&manager_props, nullptr)) {
    LOG(ERROR) << "Unable to get manager properties";
    return false;
  }

  auto it = manager_props.find(shill::kDefaultServiceProperty);
  if (it == manager_props.end()) {
    LOG(ERROR) << "Manager properties is missing default service";
    return false;
  }

  dbus::ObjectPath service_path = it->second.TryGet<dbus::ObjectPath>();
  // A path of "/" indicates that there is no default service.
  // Allow returning empty nameservers here since there's no network
  // connectivity anyway.
  if (!service_path.IsValid() || service_path.value() == "/") {
    return true;
  }

  org::chromium::flimflam::ServiceProxy service_proxy(bus_, service_path);
  brillo::VariantDictionary service_props;
  if (!service_proxy.GetProperties(&service_props, nullptr)) {
    LOG(ERROR) << "Can't retrieve properties for service";
    return false;
  }

  it = service_props.find(shill::kStateProperty);
  if (it == service_props.end()) {
    LOG(ERROR) << "Service properties are missing state";
    return false;
  }
  std::string state = it->second.TryGet<std::string>();
  if (state != shill::kStateOnline) {
    // Found the default service, but it is still connecting.
    return true;
  }

  it = service_props.find(shill::kIPConfigProperty);
  if (it == service_props.end()) {
    LOG(ERROR) << "Service properties are missing IP config";
    return false;
  }

  dbus::ObjectPath ipconfig_path = it->second.TryGet<dbus::ObjectPath>();
  if (!ipconfig_path.IsValid()) {
    LOG(ERROR) << "Invalid ipconfig path";
    return false;
  }

  org::chromium::flimflam::IPConfigProxy ipconfig_proxy(bus_, ipconfig_path);
  brillo::VariantDictionary ipconfig_props;
  if (!ipconfig_proxy.GetProperties(&ipconfig_props, nullptr)) {
    LOG(ERROR) << "Can't retrieve properties for ipconfig";
    return false;
  }

  it = ipconfig_props.find(shill::kNameServersProperty);
  if (it == ipconfig_props.end()) {
    LOG(ERROR) << "IPConfig properties are missing nameservers list";
    return false;
  }

  // Nameservers are required.
  std::vector<std::string> new_nameservers =
      it->second.TryGet<std::vector<std::string>>();
  if (new_nameservers.empty()) {
    LOG(ERROR) << "IPConfig nameservers list is empty";
    return false;
  }

  // Search domains can be omitted.
  it = ipconfig_props.find(shill::kSearchDomainsProperty);
  if (it == ipconfig_props.end()) {
    LOG(ERROR) << "IPConfig properties are missing search domains list";
    return false;
  }
  std::vector<std::string> new_search_domains =
      it->second.TryGet<std::vector<std::string>>();

  *nameservers = std::move(new_nameservers);
  *search_domains = std::move(new_search_domains);
  return true;
}

void ShillClient::OnManagerPropertyChangeRegistration(
    const std::string& interface,
    const std::string& signal_name,
    bool success) {
  CHECK(success) << "Unable to register for interface change events";
}

void ShillClient::OnManagerPropertyChange(const std::string& property_name,
                                          const brillo::Any& property_value) {
  if (property_name != shill::kDefaultServiceProperty &&
      property_name != shill::kConnectionStateProperty) {
    return;
  }

  std::vector<std::string> new_nameservers;
  std::vector<std::string> new_search_domains;
  if (!GetResolvConfig(&new_nameservers, &new_search_domains) ||
      (nameservers_ == new_nameservers &&
       search_domains_ == new_search_domains)) {
    return;
  }

  nameservers_ = std::move(new_nameservers);
  search_domains_ = std::move(new_search_domains);

  if (!config_changed_callback_.is_null()) {
    config_changed_callback_.Run(nameservers_, search_domains_);
  }
}

void ShillClient::RegisterResolvConfigChangedHandler(
    base::Callback<void(std::vector<std::string>, std::vector<std::string>)>
        callback) {
  config_changed_callback_ = std::move(callback);
  if (!GetResolvConfig(&nameservers_, &search_domains_)) {
    return;
  }
  CHECK(!config_changed_callback_.is_null());
  config_changed_callback_.Run(nameservers_, search_domains_);
}

}  // namespace concierge
}  // namespace vm_tools
