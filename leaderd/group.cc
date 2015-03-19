// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/group.h"

#include <arpa/inet.h>
#include <tuple>
#include <vector>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <base/json/json_writer.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/errors/error.h>
#include <chromeos/http/http_request.h>
#include <chromeos/http/http_utils.h>
#include <chromeos/mime_utils.h>
#include <dbus/bus.h>

#include "leaderd/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::DBusServiceWatcher;
using chromeos::dbus_utils::ExportedObjectManager;

namespace leaderd {

namespace {

const char kLeadershipScoreKey[] = "score";
const char kLeadershipGroupKey[] = "group";
const char kLeadershipIdKey[] = "uuid";
const char kLeadershipLeaderKey[] = "leader";
const char kLeadershipChallengeUri[] =
    "http://%s:%d/privet/v3/leadership/challenge";
const unsigned kWandererTimeSec = 10u;
const unsigned kHttpConnectionTimeoutMs = 10 * 1000;

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
  AddPeer(delegate_->GetUUID());
}

void Group::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(
      sequencer->GetHandler("Failed exporting DBusManager.", true));
  sequencer->OnAllTasksCompletedCall({completion_callback});
  transport_ = chromeos::http::Transport::CreateDefault();
  transport_->SetDefaultTimeout(
      base::TimeDelta::FromMilliseconds(kHttpConnectionTimeoutMs));
  Reelect();
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

void Group::HandleLeaderChallenge(const std::string& uuid, int score,
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

void Group::AddPeer(const std::string& uuid) {
  peers_.insert(uuid);
  dbus_adaptor_.SetMemberUUIDs({peers_.begin(), peers_.end()});
  AskPeerForLeaderInfo(uuid);
}

void Group::RemovePeer(const std::string& uuid) {
  peers_.erase(uuid);
  dbus_adaptor_.SetMemberUUIDs({peers_.begin(), peers_.end()});
  if (uuid == leader_) {
    Reelect();
  }
}

void Group::ClearPeers() {
  // This occurs when peerd crashes.
  peers_.clear();
  dbus_adaptor_.SetMemberUUIDs({peers_.begin(), peers_.end()});
  Reelect();
}

void Group::Reelect() {
  SetRole(State::WANDERER, "");
  wanderer_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromSeconds(kWandererTimeSec), this,
                        &Group::OnWandererTimeout);
  for (const auto& peer : peers_) {
    AskPeerForLeaderInfo(peer);
  }
}

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
  wanderer_timer_.Stop();
  dbus_adaptor_.SetLeaderUUID(leader_);
  LOG(INFO) << "Leader is now " << leader_ << " state " << state_;
}

void Group::OnWandererTimeout() {
  LOG(INFO) << "Assuming leadership role after timeout ";
  SetRole(State::LEADER, delegate_->GetUUID());
}

void Group::AskPeerForLeaderInfo(const std::string& peer_uuid) {
  if (peer_uuid == delegate_->GetUUID()) {
    // Don't look at our own peer uuids.
    return;
  }

  if (state_ == State::WANDERER || state_ == State::FOLLOWER) {
    SendLeaderChallenge(peer_uuid,
                        base::Bind(&Group::HandleLeaderChallengeResponse,
                                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void Group::GetLeaderChallengeText(std::string* text,
                                   std::string* mime_type) const {
  std::unique_ptr<base::DictionaryValue> output(new base::DictionaryValue);
  output->SetInteger(kLeadershipScoreKey, score_);
  output->SetString(kLeadershipGroupKey, guid_);
  output->SetString(kLeadershipIdKey, delegate_->GetUUID());

  base::JSONWriter::Write(output.get(), text);

  *mime_type = chromeos::mime::AppendParameter(
      chromeos::mime::application::kJson, chromeos::mime::parameters::kCharset,
      "utf-8");
}

void Group::SendLeaderChallenge(
    const std::string& peer_uuid,
    const chromeos::http::SuccessCallback& success_callback) {
  const std::vector<std::tuple<std::vector<uint8_t>, uint16_t>>& ips =
      delegate_->GetIPInfo(peer_uuid);
  if (ips.empty()) {
    VLOG(1) << "Didn't find any hosts for peer=" << peer_uuid;
    return;
  }

  std::string text, mime_type;
  GetLeaderChallengeText(&text, &mime_type);

  for (const auto& ip_port_pair : ips) {
    char address[INET6_ADDRSTRLEN];
    if (inet_ntop(std::get<0>(ip_port_pair).size() == 4 ? AF_INET : AF_INET6,
                  std::get<0>(ip_port_pair).data(), address,
                  INET6_ADDRSTRLEN) == nullptr)
      continue;

    std::string url = base::StringPrintf(kLeadershipChallengeUri, address,
                                         std::get<1>(ip_port_pair));
    VLOG(1) << "Connecting to " << url;

    chromeos::http::SendRequest(
        chromeos::http::request_type::kPost, url, text.c_str(), text.size(),
        mime_type, {}, transport_,
        base::Bind(&Group::HandleLeaderChallengeResponse,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&Group::HandleLeaderChallengeError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void Group::HandleLeaderChallengeError(int request_id,
                                       const chromeos::Error* error) {
  VLOG(1) << "Got error callback " << error->GetDomain() << ", "
          << error->GetCode() << ", " << error->GetMessage();
}

void Group::HandleLeaderChallengeResponse(
    int request_id, scoped_ptr<chromeos::http::Response> response) {
  chromeos::ErrorPtr error;
  std::unique_ptr<base::DictionaryValue> json_resp =
      chromeos::http::ParseJsonResponse(response.get(), nullptr, &error);
  if (!json_resp) {
    return;
  }

  VLOG(1) << "Got leadership response";

  std::string leader;
  std::string id;
  if (!json_resp->GetString(kLeadershipLeaderKey, &leader) ||
      !json_resp->GetString(kLeadershipIdKey, &id)) {
    return;
  }

  if (!leader.empty()) {
    if (leader == delegate_->GetUUID()) {
      SetRole(State::LEADER, leader);
    } else {
      // Is this an authoritative answer or a gossip? If it
      // is a gossip we need to connect to the leader to verify.
      if (id != leader) {
        // We can follow the redirect but we need to ensure we
        // don't get into a cycle.
        // SendLeaderChallenge(leader_service_proxy);
      } else {
        SetRole(State::FOLLOWER, leader);
      }
    }
  }
}

}  // namespace leaderd
