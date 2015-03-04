// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/manager.h"

#include <base/logging.h>

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;
using org::chromium::leaderd::ManagerAdaptor;
using dbus::ObjectPath;

namespace leaderd {

namespace {

const char kPingResponse[] = "Hello world!";

}  // namespace

Manager::Manager(const scoped_refptr<dbus::Bus>& bus,
                 ExportedObjectManager* object_manager,
                 std::unique_ptr<PeerdClient> peerd_client)
    : bus_(bus),
      dbus_object_{object_manager, bus, ManagerAdaptor::GetObjectPath()},
      peerd_client_(std::move(peerd_client)) {
  peerd_client_->SetDelegate(this);
}

Manager::~Manager() {}

void Manager::RegisterAsync(
    chromeos::dbus_utils::AsyncEventSequencer* sequencer) {
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(
      sequencer->GetHandler("Failed exporting DBusManager.", true));
}

bool Manager::JoinGroup(chromeos::ErrorPtr* error, dbus::Message* message,
                        const std::string& group_guid,
                        const std::map<std::string, chromeos::Any>& options,
                        dbus::ObjectPath* out_object_path) {
  return true;
}

std::string Manager::Ping() { return kPingResponse; }

void Manager::SetWebServerPort(uint16_t port) { web_port_ = port; }

bool Manager::ChallengeLeader(const std::string& in_uuid,
                              const std::string& in_guid, int32_t in_score,
                              std::string* out_leader,
                              std::string* out_my_uuid) {
  VLOG(1) << "Challenge Leader group='" << in_guid << "', uuid='" << in_uuid
          << "', score=" << in_score;
  auto it = groups_.find(in_guid);
  if (it == groups_.end()) {
    VLOG(1) << "Group not found";
    return false;
  }

  it->second->ChallengeLeader(in_uuid, in_score, out_leader, out_my_uuid);
  return true;
}

void Manager::OnPeerdManagerAdded(
    org::chromium::peerd::ManagerProxyInterface* manager_proxy) {
  VLOG(1) << "Peerd manager added.";
}

void Manager::OnPeerdManagerRemoved(const dbus::ObjectPath& object_path) {
  VLOG(1) << "Peerd manager removed '" << object_path.value() << "'.";
}

void Manager::OnPeerdPeerAdded(
    org::chromium::peerd::PeerProxyInterface* peer_proxy,
    const dbus::ObjectPath& object_path) {
  VLOG(1) << "Peerd peer added '" << object_path.value() << "'.";
}

void Manager::OnPeerdPeerRemoved(const dbus::ObjectPath& object_path) {
  VLOG(1) << "Peerd peer removed '" << object_path.value() << "'.";
}

void Manager::OnPeerdServiceAdded(
    org::chromium::peerd::ServiceProxyInterface* service_proxy,
    const dbus::ObjectPath& object_path) {
  VLOG(1) << "Peerd service added '" << object_path.value() << "'.";
}

void Manager::OnPeerdServiceRemoved(const dbus::ObjectPath& object_path) {
  VLOG(1) << "Peerd service removed '" << object_path.value() << "'.";
}

void Manager::OnPeerdServiceChanged(
    org::chromium::peerd::ServiceProxyInterface* service_proxy,
    const dbus::ObjectPath& object_path, const std::string& property) {
  VLOG(1) << "Peerd service update '" << object_path.value() << "', '"
          << service_proxy->service_id() << "'";
}

}  // namespace leaderd
