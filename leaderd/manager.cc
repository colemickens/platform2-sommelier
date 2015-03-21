// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/manager.h"

#include <base/bind.h>
#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "leaderd/group.h"

using chromeos::ErrorPtr;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using org::chromium::leaderd::ManagerAdaptor;
using dbus::ObjectPath;

namespace leaderd {

namespace {

const char kGroupObjectPathFormat[] = "/org/chromium/leaderd/groups/%" PRIuS;
const char kEmptyGroupId[] = "manager.empty_group";
const char kLeaderdErrorDomain[] = "leaderd";
const char kPingResponse[] = "Hello world!";

void DoneCallback(bool success) { VLOG(1) << "Done register " << success; }

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

void Manager::RemoveGroup(const std::string& group) {
  groups_.erase(group);
  if (groups_.empty()) {
    peerd_client_->StopMonitoring();
  }
  PublishService();
}

const std::string& Manager::GetUUID() const { return uuid_; }

std::vector<std::tuple<std::vector<uint8_t>, uint16_t>> Manager::GetIPInfo(
    const std::string& peer_uuid) const {
  return peerd_client_->GetIPInfo(peer_uuid);
}

void Manager::SetWebServerPort(uint16_t port) {
  web_port_ = port;
  PublishService();
}

bool Manager::HandleLeaderChallenge(const std::string& group_id,
                                    const std::string& challenger_id,
                                    int32_t challenger_score,
                                    std::string* leader_id,
                                    std::string* responder_id) {
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    VLOG(1) << "Received challenge for an unknown group.";
    return false;
  }

  it->second->HandleLeaderChallenge(challenger_id, challenger_score,
                                    leader_id, responder_id);
  return true;
}

bool Manager::HandleLeaderAnnouncement(const std::string& group_id,
                                       const std::string& leader_id,
                                       int32_t leader_score) {
  return true;
}

bool Manager::JoinGroup(chromeos::ErrorPtr* error, dbus::Message* message,
                        const std::string& in_group_id,
                        const std::map<std::string, chromeos::Any>& options,
                        dbus::ObjectPath* out_group_path) {
  const std::string& dbus_client = message->GetSender();
  LOG(INFO) << "Join group=" << in_group_id << " from " << dbus_client;
  if (in_group_id.empty()) {
    chromeos::Error::AddTo(error, FROM_HERE, kLeaderdErrorDomain, kEmptyGroupId,
                           "Expected non-empty group id.");
    return false;
  }
  auto it = groups_.find(in_group_id);
  if (it != groups_.end()) {
    *out_group_path = it->second->GetObjectPath();
    return true;
  }

  peerd_client_->StartMonitoring();

  dbus::ObjectPath path{
      base::StringPrintf(kGroupObjectPathFormat, ++last_group_dbus_id_)};
  std::unique_ptr<Group> group{new Group{
      in_group_id, bus_, dbus_object_.GetObjectManager().get(), path,
      dbus_client, peerd_client_->GetPeersMatchingGroup(in_group_id), this}};
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  group->RegisterAsync(sequencer->GetHandler("Failed to expose Group.", true));
  sequencer->OnAllTasksCompletedCall({base::Bind(&DoneCallback)});

  groups_[in_group_id].swap(group);
  *out_group_path = path;

  PublishService();
  return true;
}

std::string Manager::Ping() { return kPingResponse; }

void Manager::OnPeerdAvailable() { PublishService(); }

void Manager::OnPeerdDeath() {
  for (auto& group : groups_) {
    group.second->ClearPeers();
  }
}

void Manager::OnSelfIdChanged(const std::string& uuid) {
  if (uuid_ == uuid) {
    return;
  }
  VLOG(1) << "Setting leaderd identity to " << uuid;
  std::string old_uuid = uuid_;
  uuid_ = uuid;
  for (const auto& joined_group : groups_) {
    if (!old_uuid.empty()) {
      joined_group.second->RemovePeer(old_uuid);
    }
    if (!uuid_.empty()) {
      joined_group.second->AddPeer(uuid_);
    }
  }
}

void Manager::OnPeerGroupsChanged(const std::string& peer_uuid,
                                  const std::set<std::string>& groups) {
  // Tell all the groups about the updated peer
  for (const auto& joined_group : groups_) {
    if (groups.find(joined_group.first) == groups.end()) {
      joined_group.second->RemovePeer(peer_uuid);
    } else {
      joined_group.second->AddPeer(peer_uuid);
    }
  }
}

void Manager::PublishService() {
  if (!web_port_) {
    return;
  }
  std::vector<std::string> groups;
  for (const auto& group : groups_) {
    groups.push_back(group.first);
  }

  peerd_client_->PublishGroups(web_port_, groups);
}

}  // namespace leaderd
