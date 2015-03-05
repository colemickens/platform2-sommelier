// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/group.h"

#include <base/message_loop/message_loop.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/errors/error.h>
#include <dbus/bus.h>

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::DBusServiceWatcher;
using chromeos::dbus_utils::ExportedObjectManager;

namespace leaderd {

namespace {

const char* GroupStateToString(Group::State state) {
  switch (state) {
    case Group::State::WANDERER:
      return "WANDERER";
    case Group::State::LEADER:
      return "LEADER";
    case Group::State::FOLLOWER:
      return "FOLLOWER";
  }
  return "UNDEFINED";
}

std::ostream& operator<<(std::ostream& stream, Group::State state) {
  stream << GroupStateToString(state);
  return stream;
}

}  // namespace

Group::Group(const std::string& guid, const scoped_refptr<dbus::Bus>& bus,
             ExportedObjectManager* object_manager,
             const dbus::ObjectPath& path,
             const std::string& dbus_connection_id,
             const std::set<std::string>& peers, Delegate* delegate)
    : guid_(guid),
      object_path_(path),
      peers_(peers),
      delegate_(delegate),
      dbus_object_(object_manager, bus, path) {
  service_watcher_.reset(new DBusServiceWatcher(
      bus, dbus_connection_id,
      base::Bind(&Group::OnDBusServiceDeath, weak_ptr_factory_.GetWeakPtr())));
  Reelect();
}

void Group::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(
      sequencer->GetHandler("Failed exporting DBusManager.", true));
  sequencer->OnAllTasksCompletedCall({completion_callback});
}

const dbus::ObjectPath& Group::GetObjectPath() const { return object_path_; }

bool Group::LeaveGroup(chromeos::ErrorPtr* error) {
  RemoveSoon();
  return true;
}

bool Group::SetScore(chromeos::ErrorPtr* error, int32_t in_score) {
  score_ = in_score;
  return true;
}

bool Group::PokeLeader(chromeos::ErrorPtr* error) {
  Reelect();
  return true;
}

void Group::ChallengeLeader(const std::string& uuid, int score,
                            std::string* leader, std::string* my_uuid) {
  VLOG(1) << "Challenge leader " << state_ << ", '" << guid_ << "' , uuid '"
          << uuid << "', score " << score;

  if (state_ == State::WANDERER || state_ == State::LEADER) {
    if (IsScoreGreater(score, uuid)) {
      SetRole(State::FOLLOWER, uuid);
    } else {
      SetRole(State::LEADER, delegate_->GetUUID());
    }
  }

  *leader = leader_;
  *my_uuid = delegate_->GetUUID();
}

void Group::AddPeer(const std::string& uuid) { peers_.insert(uuid); }

void Group::RemovePeer(const std::string& uuid) {
  peers_.erase(uuid);
  if (uuid == leader_) {
    Reelect();
  }
}

void Group::ClearPeers() {
  // This occurs when peerd crashes.
  peers_.clear();
  Reelect();
}

void Group::Reelect() { SetRole(State::WANDERER, ""); }

void Group::OnDBusServiceDeath() {
  VLOG(1) << "Group removing due to death";
  RemoveSoon();
}

void Group::RemoveSoon() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Group::RemoveNow, weak_ptr_factory_.GetWeakPtr()));
}

void Group::RemoveNow() { delegate_->RemoveGroup(guid_); }

bool Group::IsScoreGreater(int score, const std::string& guid) const {
  return score > score_ || guid > delegate_->GetUUID();
}

void Group::SetRole(State state, const std::string& leader) {
  leader_ = leader;
  state_ = state;
  dbus_adaptor_.SetLeaderUUID(leader_);
  LOG(INFO) << "Leader is now " << leader_ << " state " << state_;
}

}  // namespace leaderd
