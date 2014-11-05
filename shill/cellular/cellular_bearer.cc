// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/cellular_bearer.h"

#include <ModemManager/ModemManager.h>

#include <base/bind.h>

#include "shill/dbus_properties.h"
#include "shill/dbus_properties_proxy.h"
#include "shill/logging.h"
#include "shill/proxy_factory.h"

using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kCellular;
static string ObjectID(const CellularBearer *c) { return "(cellular_bearer)"; }
}

namespace {

const char kPropertyAddress[] = "address";
const char kPropertyDNS1[] = "dns1";
const char kPropertyDNS2[] = "dns2";
const char kPropertyDNS3[] = "dns3";
const char kPropertyGateway[] = "gateway";
const char kPropertyMethod[] = "method";
const char kPropertyPrefix[] = "prefix";

IPConfig::Method ConvertMMBearerIPConfigMethod(uint32_t method) {
  switch (method) {
    case MM_BEARER_IP_METHOD_PPP:
      return IPConfig::kMethodPPP;
    case MM_BEARER_IP_METHOD_STATIC:
      return IPConfig::kMethodStatic;
    case MM_BEARER_IP_METHOD_DHCP:
      return IPConfig::kMethodDHCP;
    default:
      return IPConfig::kMethodUnknown;
  }
}

}  // namespace

CellularBearer::CellularBearer(ProxyFactory *proxy_factory,
                               const string &dbus_path,
                               const string &dbus_service)
    : proxy_factory_(proxy_factory),
      dbus_path_(dbus_path),
      dbus_service_(dbus_service),
      connected_(false),
      ipv4_config_method_(IPConfig::kMethodUnknown),
      ipv6_config_method_(IPConfig::kMethodUnknown) {
  CHECK(proxy_factory_);
}

CellularBearer::~CellularBearer() {}

bool CellularBearer::Init() {
  SLOG(this, 3) << __func__ << ": path='" << dbus_path_
                << "', service='" << dbus_service_ << "'";

  dbus_properties_proxy_.reset(
      proxy_factory_->CreateDBusPropertiesProxy(dbus_path_, dbus_service_));
  // It is possible that ProxyFactory::CreateDBusPropertiesProxy() returns
  // nullptr as the bearer DBus object may no longer exist.
  if (!dbus_properties_proxy_) {
    LOG(WARNING) << "Failed to create DBus properties proxy for bearer '"
                 << dbus_path_ << "'. Bearer is likely gone.";
    return false;
  }

  dbus_properties_proxy_->set_properties_changed_callback(base::Bind(
      &CellularBearer::OnDBusPropertiesChanged, base::Unretained(this)));
  UpdateProperties();
  return true;
}

void CellularBearer::GetIPConfigMethodAndProperties(
    const DBusPropertiesMap &properties,
    IPAddress::Family address_family,
    IPConfig::Method *ipconfig_method,
    std::unique_ptr<IPConfig::Properties> *ipconfig_properties) const {
  DCHECK(ipconfig_method);
  DCHECK(ipconfig_properties);

  uint32_t method;
  if (!DBusProperties::GetUint32(properties, kPropertyMethod, &method)) {
    SLOG(this, 2) << "Bearer '" << dbus_path_
                  << "' does not specify an IP configuration method.";
    method = MM_BEARER_IP_METHOD_UNKNOWN;
  }
  *ipconfig_method = ConvertMMBearerIPConfigMethod(method);
  ipconfig_properties->reset();

  if (*ipconfig_method != IPConfig::kMethodStatic)
    return;

  string address, gateway;
  if (!DBusProperties::GetString(properties, kPropertyAddress, &address) ||
      !DBusProperties::GetString(properties, kPropertyGateway, &gateway)) {
    SLOG(this, 2) << "Bearer '" << dbus_path_
                  << "' static IP configuration does not specify valid "
                         "address/gateway information.";
    *ipconfig_method = IPConfig::kMethodUnknown;
    return;
  }

  ipconfig_properties->reset(new IPConfig::Properties);
  (*ipconfig_properties)->address_family = address_family;
  (*ipconfig_properties)->address = address;
  (*ipconfig_properties)->gateway = gateway;

  uint32_t prefix;
  if (!DBusProperties::GetUint32(properties, kPropertyPrefix, &prefix)) {
    prefix = IPAddress::GetMaxPrefixLength(address_family);
  }
  (*ipconfig_properties)->subnet_prefix = prefix;

  string dns;
  if (DBusProperties::GetString(properties, kPropertyDNS1, &dns)) {
    (*ipconfig_properties)->dns_servers.push_back(dns);
  }
  if (DBusProperties::GetString(properties, kPropertyDNS2, &dns)) {
    (*ipconfig_properties)->dns_servers.push_back(dns);
  }
  if (DBusProperties::GetString(properties, kPropertyDNS3, &dns)) {
    (*ipconfig_properties)->dns_servers.push_back(dns);
  }
}

void CellularBearer::ResetProperties() {
  connected_ = false;
  data_interface_.clear();
  ipv4_config_method_ = IPConfig::kMethodUnknown;
  ipv4_config_properties_.reset();
  ipv6_config_method_ = IPConfig::kMethodUnknown;
  ipv6_config_properties_.reset();
}

void CellularBearer::UpdateProperties() {
  ResetProperties();

  if (!dbus_properties_proxy_)
    return;

  DBusPropertiesMap properties =
      dbus_properties_proxy_->GetAll(MM_DBUS_INTERFACE_BEARER);
  if (properties.empty()) {
    LOG(WARNING) << "Could not get properties of bearer '" << dbus_path_
                 << "'. Bearer is likely gone and thus ignored.";
    return;
  }

  OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                          vector<string>());
}

void CellularBearer::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &/*invalidated_properties*/) {
  SLOG(this, 3) << __func__ << ": path=" << dbus_path_
                << ", interface=" << interface;

  if (interface != MM_DBUS_INTERFACE_BEARER)
    return;

  bool connected;
  if (DBusProperties::GetBool(
        changed_properties, MM_BEARER_PROPERTY_CONNECTED, &connected)) {
    connected_ = connected;
  }

  string data_interface;
  if (DBusProperties::GetString(
        changed_properties, MM_BEARER_PROPERTY_INTERFACE, &data_interface)) {
    data_interface_ = data_interface;
  }

  DBusPropertiesMap ipconfig;
  if (DBusProperties::GetDBusPropertiesMap(
        changed_properties, MM_BEARER_PROPERTY_IP4CONFIG, &ipconfig)) {
    GetIPConfigMethodAndProperties(ipconfig,
                                   IPAddress::kFamilyIPv4,
                                   &ipv4_config_method_,
                                   &ipv4_config_properties_);
  }
  if (DBusProperties::GetDBusPropertiesMap(
        changed_properties, MM_BEARER_PROPERTY_IP6CONFIG, &ipconfig)) {
    GetIPConfigMethodAndProperties(ipconfig,
                                   IPAddress::kFamilyIPv6,
                                   &ipv6_config_method_,
                                   &ipv6_config_properties_);
  }
}

}  // namespace shill
