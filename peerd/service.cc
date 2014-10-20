// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/service.h"

#include <base/strings/string_util.h>
#include <chromeos/dbus/exported_object_manager.h>

#include "peerd/dbus_constants.h"

using chromeos::Error;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusInterface;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;
using dbus::ObjectPath;
using peerd::dbus_constants::kServiceId;
using peerd::dbus_constants::kServiceInfo;
using peerd::dbus_constants::kServiceInterface;
using peerd::dbus_constants::kServiceIpInfos;
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

}  // namespace service
}  // namespace errors

Service::Service(const scoped_refptr<dbus::Bus>& bus,
                 chromeos::dbus_utils::ExportedObjectManager* object_manager,
                 const dbus::ObjectPath& path)
    : dbus_object_(new DBusObject{object_manager, bus, path}) {
}

bool Service::RegisterAsync(chromeos::ErrorPtr* error,
                            const std::string& service_id,
                            const IpAddresses& addresses,
                            const ServiceInfo& service_info,
                            const CompletionAction& completion_callback) {
  if (!IsValidServiceId(error, service_id)) { return false; }
  if (!IsValidServiceInfo(error, service_info)) { return false; }
  service_id_.SetValue(service_id);
  ip_addresses_.SetValue(addresses);
  service_info_.SetValue(service_info);
  DBusInterface* itf = dbus_object_->AddOrGetInterface(kServiceInterface);
  itf->AddProperty(kServiceId, &service_id_);
  itf->AddProperty(kServiceIpInfos, &ip_addresses_);
  itf->AddProperty(kServiceInfo, &service_info_);
  dbus_object_->RegisterAsync(completion_callback);
  return true;
}

const std::string& Service::GetServiceId() const {
  return service_id_.value();
}

const Service::IpAddresses& Service::GetIpAddresses() const {
  return ip_addresses_.value();
}

const Service::ServiceInfo& Service::GetServiceInfo() const {
  return service_info_.value();
}

bool Service::Update(chromeos::ErrorPtr* error,
                     const IpAddresses& addresses,
                     const ServiceInfo& info) {
  if (!IsValidServiceInfo(error, info)) {
    return false;
  }
  ip_addresses_.SetValue(addresses);
  service_info_.SetValue(info);
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
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceId,
                 "Invalid service ID length.");
    return false;
  }
  if (!base::ContainsOnlyChars(service_id, kValidServiceIdCharacters)) {
    Error::AddTo(error,
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceId,
                 "Invalid character in service ID.");
    return false;
  }
  if (service_id.front() == '-' || service_id.back() == '-') {
    Error::AddTo(error,
                 kPeerdErrorDomain,
                 errors::service::kInvalidServiceId,
                 "Service ID may not start or end with hyphens.");
    return false;
  }
  if (service_id.find("--") != string::npos) {
    Error::AddTo(error,
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
                   kPeerdErrorDomain,
                   errors::service::kInvalidServiceInfo,
                   "Invalid service info pair length.");
      return false;
    }
    if (!base::ContainsOnlyChars(kv.first, kValidServiceInfoKeyCharacters)) {
      Error::AddTo(error,
                   kPeerdErrorDomain,
                   errors::service::kInvalidServiceInfo,
                   "Invalid service key.");
      return false;
    }
  }
  return true;
}

}  // namespace peerd
