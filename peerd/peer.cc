// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/peer.h"

#include <functional>

#include <base/format_macros.h>
#include <base/guid.h>
#include <base/memory/weak_ptr.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/exported_object_manager.h>

#include "peerd/dbus_constants.h"
#include "peerd/typedefs.h"

using base::WeakPtr;
using chromeos::Error;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusInterface;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;
using dbus::ObjectPath;
using peerd::dbus_constants::kPeerFriendlyName;
using peerd::dbus_constants::kPeerInterface;
using peerd::dbus_constants::kPeerLastSeen;
using peerd::dbus_constants::kPeerNote;
using peerd::dbus_constants::kPeerUUID;
using peerd::dbus_constants::kServicePathFragment;
using std::map;
using std::string;
using std::vector;
using std::unique_ptr;

namespace {

const size_t kMaxFriendlyNameLength = 31;
const size_t kMaxNoteLength = 255;
const char kValidFriendlyNameCharacters[] = "abcdefghijklmnopqrstuvwxyz"
                                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "0123456789"
                                            "_-,.?! ";
const char kValidNoteCharacters[] = "abcdefghijklmnopqrstuvwxyz"
                                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "0123456789"
                                    "_-,.?! ";

}  // namespace

namespace peerd {

namespace errors {
namespace peer {

const char kInvalidUUID[] = "peer.uuid";
const char kInvalidName[] = "peer.name";
const char kInvalidNote[] = "peer.note";

}  // namespace peer
}  // namespace errors

unique_ptr<Peer> Peer::MakePeer(
    chromeos::ErrorPtr* error,
    ExportedObjectManager* object_manager,
    const ObjectPath& path,
    const std::string& uuid,
    const std::string& friendly_name,
    const std::string& note,
    uint64_t last_seen,
    const CompletionAction& completion_callback) {
  CHECK(object_manager) << "object_manager is nullptr!";
  ObjectPath service_path_prefix(path.value() + kServicePathFragment);
  unique_ptr<DBusObject> dbus_object(
      new DBusObject(object_manager, object_manager->GetBus(), path));
  if (!base::IsValidGUID(uuid)) {
    Error::AddTo(error,
                 kPeerdErrorDomain,
                 errors::peer::kInvalidUUID,
                 "Invalid UUID for peer.");
    return nullptr;
  }
  unique_ptr<Peer> result(new Peer(std::move(dbus_object),
                                   service_path_prefix,
                                   uuid));
  if (!result->SetFriendlyName(error, friendly_name)) { return nullptr; }
  if (!result->SetNote(error, note)) { return nullptr; }
  result->SetLastSeen(last_seen);
  result->RegisterAsync(completion_callback);
  return result;
}

Peer::Peer(std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object,
           const ObjectPath& service_path_prefix,
           const std::string& uuid)
    : dbus_object_(std::move(dbus_object)),
      service_path_prefix_(service_path_prefix) {
  uuid_.SetValue(uuid);
}

void Peer::RegisterAsync(const CompletionAction& completion_callback) {
  DBusInterface* itf = dbus_object_->AddOrGetInterface(kPeerInterface);
  itf->AddProperty(kPeerUUID, &uuid_);
  itf->AddProperty(kPeerFriendlyName, &name_);
  itf->AddProperty(kPeerNote, &note_);
  itf->AddProperty(kPeerLastSeen, &last_seen_);
  dbus_object_->RegisterAsync(completion_callback);
}

bool Peer::SetFriendlyName(chromeos::ErrorPtr* error,
                           const string& friendly_name) {
  if (friendly_name.length() > kMaxFriendlyNameLength) {
    Error::AddToPrintf(error, kPeerdErrorDomain, errors::peer::kInvalidName,
                       "Bad length for %s: %" PRIuS,
                       kPeerFriendlyName, friendly_name.length());
    return false;
  }
  if (!base::ContainsOnlyChars(friendly_name, kValidFriendlyNameCharacters)) {
    Error::AddToPrintf(error, kPeerdErrorDomain, errors::peer::kInvalidName,
                       "Invalid characters in %s.", kPeerFriendlyName);
    return false;
  }
  name_.SetValue(friendly_name);
  return true;
}

bool Peer::SetNote(chromeos::ErrorPtr* error, const string& note) {
  if (note.length() > kMaxNoteLength) {
    Error::AddToPrintf(error, kPeerdErrorDomain, errors::peer::kInvalidNote,
                       "Bad length for %s: %" PRIuS,
                       kPeerNote, note.length());
    return false;
  }
  if (!base::ContainsOnlyChars(note, kValidNoteCharacters)) {
    Error::AddToPrintf(error, kPeerdErrorDomain, errors::peer::kInvalidNote,
                       "Invalid characters in %s.", kPeerNote);
    return false;
  }
  note_.SetValue(note);
  return true;
}

void Peer::SetLastSeen(uint64_t last_seen) {
  last_seen_.SetValue(last_seen);
}

std::string Peer::GetUUID() const {
  return uuid_.value();
}

std::string Peer::GetFriendlyName() const {
  return name_.value();
}

std::string Peer::GetNote() const {
  return note_.value();
}

bool Peer::AddService(chromeos::ErrorPtr* error,
                      const string& service_id,
                      const vector<ip_addr>& addresses,
                      const map<string, string>& service_info) {
  ObjectPath service_path(service_path_prefix_.value() +
                          std::to_string(++services_added_));
  // TODO(wiley): There is a potential race here.  If we remove the service
  //              too quickly, we race with DBus export completing.  We'll
  //              have to be careful in Service not to assume export has
  //              finished.
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  unique_ptr<Service> new_service(
      Service::MakeService(error, dbus_object_->GetObjectManager().get(),
                           service_path, service_id, addresses,
                           service_info,
                           sequencer->GetHandler("Failed exporting service.",
                                                 true)));
  if (!new_service) { return false; }
  services_.emplace(service_id, std::move(new_service));
  sequencer->OnAllTasksCompletedCall({});
  bool all_publishers_succeeded = true;
  // Notify all the publishers we know about that we have a new service.
  auto first_null = std::remove_if(
      publishers_.begin(), publishers_.end(),
      std::logical_not<base::WeakPtr<ServicePublisherInterface>>());
  publishers_.erase(first_null, publishers_.end());
  for (const auto& publisher : publishers_) {
      all_publishers_succeeded &= publisher->OnServiceUpdated(
          error, *services_[service_id]);
  }
  return all_publishers_succeeded;
}

bool Peer::RemoveService(chromeos::ErrorPtr* error,
                         const string& service_id) {
  // Notify all the publishers we know about that we have removed a service.
  bool all_publishers_succeeded = true;
  auto first_null = std::remove_if(
      publishers_.begin(), publishers_.end(),
      std::logical_not<base::WeakPtr<ServicePublisherInterface>>());
  publishers_.erase(first_null, publishers_.end());
  for (const auto& publisher : publishers_) {
      all_publishers_succeeded &= publisher->OnServiceRemoved(
          error, service_id);
  }
  CHECK(services_.erase(service_id))
      << "Tried to erase service that did not exist on this peer!";
  return all_publishers_succeeded;
}

void Peer::RegisterServicePublisher(
    WeakPtr<ServicePublisherInterface> publisher) {
  if (!publisher) { return; }
  for (const auto& kv : services_) {
    publisher->OnServiceUpdated(nullptr, *kv.second);
  }
  publishers_.push_back(publisher);
}

}  // namespace peerd
