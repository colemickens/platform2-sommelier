// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/peer.h"

#include <base/format_macros.h>
#include <base/guid.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/exported_object_manager.h>

#include "peerd/dbus_constants.h"
#include "peerd/typedefs.h"

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
  unique_ptr<DBusObject> dbus_object(
      new DBusObject(object_manager, object_manager->GetBus(), path));
  return MakePeerImpl(error, std::move(dbus_object),
                      uuid, friendly_name, note, last_seen,
                      completion_callback);
}

unique_ptr<Peer> Peer::MakePeerImpl(
    chromeos::ErrorPtr* error,
    unique_ptr<DBusObject> dbus_object,
    const string& possibly_lower_uuid,
    const string& friendly_name,
    const string& note,
    uint64_t last_seen,
    const CompletionAction& completion_callback) {
  const string uuid(StringToUpperASCII(possibly_lower_uuid));
  if (!base::IsValidGUID(uuid)) {
    Error::AddTo(error,
                 kPeerdErrorDomain,
                 errors::peer::kInvalidUUID,
                 "Invalid UUID for peer.");
    return nullptr;
  }
  unique_ptr<Peer> result(new Peer(std::move(dbus_object), uuid));
  if (!result->SetFriendlyName(error, friendly_name)) { return nullptr; }
  if (!result->SetNote(error, note)) { return nullptr; }
  result->SetLastSeen(last_seen);
  result->RegisterAsync(completion_callback);
  return result;
}

Peer::Peer(std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object,
           const std::string& uuid) : dbus_object_(std::move(dbus_object)) {
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

bool Peer::AddService(chromeos::ErrorPtr* error,
                      const string& service_id,
                      const vector<ip_addr>& addresses,
                      const map<string, string>& service_info) {
  return true;
}

bool Peer::RemoveService(chromeos::ErrorPtr* error,
                         const string& service_id) {
  return true;
}

}  // namespace peerd
