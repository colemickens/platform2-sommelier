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

unique_ptr<Service> Service::MakeService(
    chromeos::ErrorPtr* error,
    ExportedObjectManager* object_manager,
    const ObjectPath& path,
    const string& service_id,
    const IpAddresses& addresses,
    const ServiceInfo& service_info,
    const CompletionAction& completion_callback) {
  CHECK(object_manager) << "object_manager is nullptr!";
  unique_ptr<DBusObject> dbus_object(
      new DBusObject(object_manager, object_manager->GetBus(), path));
  return MakeServiceImpl(error, std::move(dbus_object),
                         service_id, addresses, service_info,
                         completion_callback);
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

unique_ptr<Service> Service::MakeServiceImpl(
    chromeos::ErrorPtr* error,
    unique_ptr<DBusObject> dbus_object,
    const string& service_id,
    const IpAddresses& addresses,
    const ServiceInfo& service_info,
    const CompletionAction& completion_callback) {
  unique_ptr<Service> result;
  if (!IsValidServiceId(error, service_id))
    return result;
  if (!IsValidServiceInfo(error, service_info))
    return result;
  result.reset(
      new Service(std::move(dbus_object), service_id, addresses, service_info));
  result->RegisterAsync(completion_callback);
  return result;
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

Service::Service(unique_ptr<DBusObject> dbus_object,
                 const string& service_id,
                 const IpAddresses& addresses,
                 const ServiceInfo& service_info)
    : dbus_object_(std::move(dbus_object)) {
  service_id_.SetValue(service_id);
  ip_addresses_.SetValue(addresses);
  service_info_.SetValue(service_info);
}

void Service::RegisterAsync(const CompletionAction& completion_callback) {
  DBusInterface* itf = dbus_object_->AddOrGetInterface(kServiceInterface);
  itf->AddProperty(kServiceId, &service_id_);
  itf->AddProperty(kServiceIpInfos, &ip_addresses_);
  itf->AddProperty(kServiceInfo, &service_id_);
  dbus_object_->RegisterAsync(completion_callback);
}

}  // namespace peerd
