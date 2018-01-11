// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/peer.h"

#include <utility>

#include <base/format_macros.h>
#include <base/guid.h>
#include <base/memory/weak_ptr.h>
#include <base/strings/string_util.h>
#include <brillo/dbus/exported_object_manager.h>

#include "peerd/dbus_constants.h"
#include "peerd/typedefs.h"

using base::Time;
using base::TimeDelta;
using brillo::Any;
using brillo::Error;
using brillo::dbus_utils::AsyncEventSequencer;
using brillo::dbus_utils::DBusObject;
using brillo::dbus_utils::ExportedObjectManager;
using dbus::ObjectPath;
using peerd::dbus_constants::kServicePathFragment;
using std::map;
using std::string;
using std::vector;
using std::unique_ptr;

namespace peerd {

namespace errors {
namespace peer {

const char kInvalidUUID[] = "peer.uuid";
const char kInvalidTime[] = "peer.time";
const char kUnknownService[] = "peer.unknown_service";
const char kDuplicateServiceID[] = "peer.duplicate_service_id";

}  // namespace peer
}  // namespace errors

Peer::Peer(const scoped_refptr<dbus::Bus>& bus,
           ExportedObjectManager* object_manager,
           const ObjectPath& path)
    : bus_(bus),
      dbus_object_(new DBusObject{object_manager, bus, path}),
      service_path_prefix_(path.value() + kServicePathFragment) {
}

bool Peer::RegisterAsync(
    brillo::ErrorPtr* error,
    const std::string& uuid,
    const Time& last_seen,
    const CompletionAction& completion_callback) {
  if (!base::IsValidGUID(uuid)) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::peer::kInvalidUUID,
                 "Invalid UUID for peer.");
    return false;
  }
  dbus_adaptor_.SetUUID(uuid);
  if (!SetLastSeen(error, last_seen)) { return false; }
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
  return true;
}

bool Peer::SetLastSeen(brillo::ErrorPtr* error, const Time& last_seen) {
  if (!IsValidUpdateTime(error, last_seen)) {
    return false;
  }
  uint64_t milliseconds_since_epoch = 0;
  CHECK(TimeToMillisecondsSinceEpoch(last_seen, &milliseconds_since_epoch));
  dbus_adaptor_.SetLastSeen(milliseconds_since_epoch);
  return true;
}

std::string Peer::GetUUID() const {
  return dbus_adaptor_.GetUUID();
}

Time Peer::GetLastSeen() const {
  return TimeDelta::FromMilliseconds(dbus_adaptor_.GetLastSeen()) +
         Time::UnixEpoch();
}

bool Peer::IsValidUpdateTime(brillo::ErrorPtr* error,
                             const base::Time& last_seen) const {
  uint64_t milliseconds_since_epoch = 0;
  if (!TimeToMillisecondsSinceEpoch(last_seen, &milliseconds_since_epoch)) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::peer::kInvalidTime,
                 "Negative time update is invalid.");
    return false;
  }
  if (milliseconds_since_epoch < dbus_adaptor_.GetLastSeen()) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::peer::kInvalidTime,
                 "Discarding update to last seen time as stale.");
    return false;
  }
  return true;
}

bool Peer::AddService(brillo::ErrorPtr* error,
                      const string& service_id,
                      const Service::IpAddresses& addresses,
                      const map<string, string>& service_info,
                      const map<string, Any>& options) {
  if (services_.find(service_id) != services_.end()) {
    Error::AddToPrintf(error,
                       FROM_HERE,
                       kPeerdErrorDomain,
                       errors::peer::kDuplicateServiceID,
                       "Cannot add service with duplicate service ID %s.",
                       service_id.c_str());
    return false;
  }
  ObjectPath service_path(service_path_prefix_.value() +
                          std::to_string(++services_added_));
  // TODO(wiley): There is a potential race here.  If we remove the service
  //              too quickly, we race with DBus export completing.  We'll
  //              have to be careful in Service not to assume export has
  //              finished.
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  unique_ptr<Service> new_service{new Service{
      bus_, dbus_object_->GetObjectManager().get(), service_path}};
  const bool success = new_service->RegisterAsync(
      error, GetUUID(), service_id, addresses, service_info, options,
      sequencer->GetHandler("Failed exporting service.", true));
  if (success) {
    services_.emplace(service_id, std::move(new_service));
    sequencer->OnAllTasksCompletedCall({});
  }
  return success;
}

bool Peer::RemoveService(brillo::ErrorPtr* error,
                         const string& service_id) {
  if (services_.erase(service_id) == 0) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::peer::kUnknownService,
                 "Unknown service id.");
    return false;
  }
  return true;
}

bool Peer::TimeToMillisecondsSinceEpoch(const Time& time, uint64_t* ret) const {
  int64_t time_diff = (time - Time::UnixEpoch()).InMilliseconds();
  if (time_diff < 0) {
    return false;
  }
  *ret = static_cast<uint64_t>(time_diff);
  return true;
}

}  // namespace peerd
