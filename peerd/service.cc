// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/service.h"

#include <limits>

#include <base/strings/string_util.h>
#include <chromeos/dbus/exported_object_manager.h>

#include "peerd/constants.h"
#include "peerd/dbus_constants.h"

using chromeos::Any;
using chromeos::Error;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;
using dbus::ObjectPath;
using peerd::constants::options::service::kMDNSPort;
using peerd::constants::options::service::kMDNSSectionName;
using std::map;
using std::string;
using std::unique_ptr;

namespace {
const char kValidServiceIdCharacters[] = "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789"
                                         "-";
const char kValidServiceInfoKeyCharacters[] = "abcdefghijklmnopqrstuvwxyz"
                                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                              "0123456789"
                                              "_";
}  // namespace


namespace peerd {

namespace errors {
namespace service {

const char kInvalidServiceId[] = "service.id";
const char kInvalidServiceInfo[] = "service.info";
const char kInvalidServiceOptions[] = "service.options";

}  // namespace service
}  // namespace errors

Service::Service(const scoped_refptr<dbus::Bus>& bus,
                 chromeos::dbus_utils::ExportedObjectManager* object_manager,
                 const dbus::ObjectPath& path)
    : dbus_object_(new DBusObject{object_manager, bus, path}) {
}

bool Service::RegisterAsync(chromeos::ErrorPtr* error,
                            const string& peer_id,
                            const string& service_id,
                            const IpAddresses& addresses,
                            const ServiceInfo& service_info,
                            const map<string, Any>& options,
                            const CompletionAction& completion_callback) {
  if (!IsValidServiceId(error, service_id)) { return false; }
  if (!Update(error, addresses, service_info, options)) { return false;}
  dbus_adaptor_.SetPeerId(peer_id);
  dbus_adaptor_.SetServiceId(service_id);
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
  return true;
}

std::string Service::GetServiceId() const {
  return dbus_adaptor_.GetServiceId();
}

Service::IpAddresses Service::GetIpAddresses() const {
  return dbus_adaptor_.GetIpInfos();
}

Service::ServiceInfo Service::GetServiceInfo() const {
  return dbus_adaptor_.GetServiceInfo();
}
const Service::MDnsOptions& Service::GetMDnsOptions() const {
  return parsed_mdns_options_;
}

bool Service::Update(chromeos::ErrorPtr* error,
                     const IpAddresses& addresses,
                     const ServiceInfo& info,
                     const map<string, Any>& options) {
  MDnsOptions mdns_options;
  if (!IsValidServiceInfo(error, info)) { return false; }
  if (!ParseOptions(error, options, &mdns_options)) { return false; }
  dbus_adaptor_.SetIpInfos(addresses);
  dbus_adaptor_.SetServiceInfo(info);
  parsed_mdns_options_ = mdns_options;
  return true;
}

bool Service::IsValidServiceId(chromeos::ErrorPtr* error,
                               const std::string& service_id) {
  // From RFC 6335 (mDNS service names):
  // Valid service names are hereby normatively defined as follows:
  //
  //   o  MUST be at least 1 character and no more than 15 characters long
  //   o  MUST contain only US-ASCII [ANSI.X3.4-1986] letters 'A' - 'Z' and
  //      'a' - 'z', digits '0' - '9', and hyphens ('-', ASCII 0x2D or
  //       decimal 45)
  //   o  MUST contain at least one letter ('A' - 'Z' or 'a' - 'z')
  //   o  MUST NOT begin or end with a hyphen
  //   o  hyphens MUST NOT be adjacent to other hyphens
  if (service_id.empty() || service_id.length() > kMaxServiceIdLength) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceId,
                 "Invalid service ID length.");
    return false;
  }
  if (!base::ContainsOnlyChars(service_id, kValidServiceIdCharacters)) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceId,
                 "Invalid character in service ID.");
    return false;
  }
  if (service_id.front() == '-' || service_id.back() == '-') {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceId,
                 "Service ID may not start or end with hyphens.");
    return false;
  }
  if (service_id.find("--") != string::npos) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceId,
                 "Service ID may not contain adjacent hyphens.");
    return false;
  }
  return true;
}

bool Service::IsValidServiceInfo(chromeos::ErrorPtr* error,
                                 const ServiceInfo& service_info) {
  for (const auto& kv : service_info) {
    if (kv.first.length() + kv.second.length() > kMaxServiceInfoPairLength) {
      Error::AddTo(error,
                   FROM_HERE,
                   kPeerdErrorDomain,
                   errors::service::kInvalidServiceInfo,
                   "Invalid service info pair length.");
      return false;
    }
    if (!base::ContainsOnlyChars(kv.first, kValidServiceInfoKeyCharacters)) {
      Error::AddTo(error,
                   FROM_HERE,
                   kPeerdErrorDomain,
                   errors::service::kInvalidServiceInfo,
                   "Invalid service key.");
      return false;
    }
  }
  return true;
}

bool Service::ParseOptions(chromeos::ErrorPtr* error,
                           const map<string, Any>& orig_options,
                           MDnsOptions* mdns_options_out) {
  map<string, Any> options{orig_options};
  auto mdns_it = options.find(kMDNSSectionName);
  if (mdns_it != options.end()) {
    if (!ExtractMDnsOptions(error, &mdns_it->second, mdns_options_out)) {
      return false;
    }
    options.erase(mdns_it);
  }
  if (!options.empty()) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceOptions,
                 "Invalid service options.");
    return false;
  }
  return true;
}

bool Service::ExtractMDnsOptions(chromeos::ErrorPtr* error,
                                 Any* maybe_mdns_options,
                                 MDnsOptions* mdns_options_out) {
  map<string, Any>* mdns_options =
      maybe_mdns_options->GetPtr<map<string, Any>>();
  if (mdns_options == nullptr) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceOptions,
                 "Invalid entry for mDNS options.");
    return false;
  }
  auto port_it = mdns_options->find(kMDNSPort);
  if (port_it != mdns_options->end()) {
    intmax_t port;
    if (!port_it->second.IsConvertibleToInteger() ||
        (port = port_it->second.GetAsInteger()) < 0 ||
        port > std::numeric_limits<uint16_t>::max()) {
      Error::AddTo(error,
                   FROM_HERE,
                   kPeerdErrorDomain,
                   errors::service::kInvalidServiceOptions,
                   "Invalid entry for mDNS port.");
      return false;
    }
    mdns_options_out->port = static_cast<uint16_t>(port);
    mdns_options->erase(port_it);
  }
  if (!mdns_options->empty()) {
      Error::AddTo(error,
                   FROM_HERE,
                   kPeerdErrorDomain,
                   errors::service::kInvalidServiceOptions,
                   "Extra entry in mDNS options.");
      return false;
  }
  return true;
}

}  // namespace peerd
