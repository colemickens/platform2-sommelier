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
using chromeos::http::Response;

namespace leaderd {

namespace {

const char kApiVerbAnnounce[] = "announce";
const char kApiVerbChallenge[] = "challenge";

const uint64_t kLeaderChallengePeriodSec = 20u;
// TODO(wiley) Devices should pick their wanderer timeouts randomly inside a
//             fixed range.
const uint64_t kWandererTimeoutSec = 10u;
const uint64_t kWandererRequeryTimeSec = 5u;
const uint64_t kLeadershipAnnouncementPeriodSec = 10u;
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

void IgnoreHttpSuccess(chromeos::http::RequestID request_id,
                       scoped_ptr<Response> response) {
}

void IgnoreHttpFailure(chromeos::http::RequestID request_id,
                       const chromeos::Error* error) {
  VLOG(1) << "HTTP request failed: " << error->GetDomain() << ", "
          << error->GetCode() << ", " << error->GetMessage();
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
      base::Bind(&Group::OnDBusServiceDeath, lifetime_factory_.GetWeakPtr())));
  AddPeer(delegate_->GetUUID());
}

void Group::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(
      sequencer->GetHandler("Failed exporting Group.", true));
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

void Group::HandleLeaderChallenge(const std::string& challenger_id,
                                  int32_t challenger_score,
                                  std::string* leader_id,
                                  std::string* my_id) {
  LOG(INFO) << "Received challenge for group='" << guid_
          << "' in state=" << state_
          << " from peer='" << challenger_id
          << "' with score=" << challenger_score;

  if (state_ == State::LEADER &&
      IsTheirScoreGreater(challenger_score, challenger_id)) {
    SetRole(State::FOLLOWER, challenger_id);
  }
  *leader_id = leader_;
  *my_id = delegate_->GetUUID();
}

bool Group::HandleLeaderAnnouncement(const std::string& leader_id,
                                     int32_t leader_score) {
  VLOG(1) << "Received announcement for group='" << guid_
          << "' in state=" << state_
          << " from peer='" << leader_id
          << "' with score=" << leader_score;
  if (peers_.find(leader_id) == peers_.end()) {
    VLOG(1) << "Ignoring announcement from unknown group member.";
    return false;
  }
  switch (state_) {
    case State::WANDERER:
      SetRole(State::FOLLOWER, leader_id);
      break;
    case State::FOLLOWER:
      if (IsTheirScoreGreater(leader_score, leader_id)) {
        // The leader has just claimed a higher score than ours.  Skip
        // challenging the leader for now.
        heartbeat_timer_->Reset();
      }
      break;
    case State::LEADER:
      // If we're a leader, and we hear from another leader, there is
      // a conflict.  Resolve this by unilaterally becoming a wanderer
      // and searching for an appropriate leader.
      SetRole(State::WANDERER, std::string{});
      break;
  }
  return true;
}

void Group::AddPeer(const std::string& uuid) {
  peers_.insert(uuid);
  dbus_adaptor_.SetMemberUUIDs({peers_.begin(), peers_.end()});
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
  SetRole(State::WANDERER, std::string{});
}

void Group::OnDBusServiceDeath() {
  VLOG(1) << "Group removing due to death";
  RemoveSoon();
}

void Group::RemoveSoon() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Group::RemoveNow, lifetime_factory_.GetWeakPtr()));
}

void Group::RemoveNow() { delegate_->RemoveGroup(guid_); }

bool Group::IsTheirScoreGreater(int32_t other_score,
                                const std::string& other_id) const {
  return other_score > score_ ||
         (other_score == score_ && other_id > delegate_->GetUUID());
}

void Group::SetRole(State state, const std::string& leader) {
  state_ = state;
  leader_ = leader;
  dbus_adaptor_.SetLeaderUUID(leader_);
  LOG(INFO) << "Leader is now " << leader_ << " state " << state_;
  wanderer_timer_->Stop();
  heartbeat_timer_->Stop();
  base::Closure heartbeat_task;
  switch (state) {
    case State::WANDERER:
      CHECK(leader.empty());
      heartbeat_task = base::Bind(
          &Group::AskPeersForLeaderInfo, per_state_factory_.GetWeakPtr());
      wanderer_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kWandererTimeoutSec),
          base::Bind(&Group::OnWandererTimeout,
                     per_state_factory_.GetWeakPtr()));
      heartbeat_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kWandererRequeryTimeSec),
          heartbeat_task);
      // No reason to wait, let's ask our peers who the leader is right away.
      heartbeat_task.Run();
      break;
    case State::FOLLOWER:
      // Periodically challenge the leader.
      heartbeat_task = base::Bind(&Group::SendLeaderChallenge,
                                  per_state_factory_.GetWeakPtr(),
                                  leader_);
      heartbeat_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kLeaderChallengePeriodSec),
          heartbeat_task);
      break;
    case State::LEADER:
      heartbeat_task = base::Bind(
          &Group::AnnounceLeadership, per_state_factory_.GetWeakPtr());
      heartbeat_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kLeadershipAnnouncementPeriodSec),
          heartbeat_task);
      // Immediately announce our leadership.
      heartbeat_task.Run();
      break;
  }
}

void Group::OnWandererTimeout() {
  LOG(INFO) << "Assuming leadership role after timeout ";
  SetRole(State::LEADER, delegate_->GetUUID());
}

bool Group::BuildApiUrls(const std::string& api_verb,
                         const std::string& peer_id,
                         std::vector<std::string>* urls) const {
  if (peer_id == delegate_->GetUUID()) {
    return false;  // Refuse to send requests to ourselves.
  }
  const IpInfo ips = delegate_->GetIPInfo(peer_id);
  if (ips.empty()) {
    VLOG(1) << "Didn't find any hosts for peer=" << peer_id;
    return false;
  }
  for (const auto& ip_port_pair : ips) {
    char address[INET6_ADDRSTRLEN];
    if (inet_ntop(std::get<0>(ip_port_pair).size() == 4 ? AF_INET : AF_INET6,
                  std::get<0>(ip_port_pair).data(), address,
                  INET6_ADDRSTRLEN) == nullptr)
      continue;
    urls->push_back(base::StringPrintf(
        "http://%s:%d/privet/v3/leadership/%s",
        address, std::get<1>(ip_port_pair), api_verb.c_str()));
  }
  return true;
}

void Group::AskPeersForLeaderInfo() {
  for (const auto& peer_id : peers_) {
    SendLeaderChallenge(peer_id);
  }
}

void Group::SendLeaderChallenge(const std::string& peer_id) {
  std::vector<std::string> urls;
  if (!BuildApiUrls(kApiVerbChallenge, peer_id, &urls)) {
    return;
  }
  base::DictionaryValue challenge_content;
  challenge_content.SetInteger(http_api::kChallengeScoreKey, score_);
  challenge_content.SetString(http_api::kChallengeGroupKey, guid_);
  challenge_content.SetString(http_api::kChallengeIdKey, delegate_->GetUUID());
  for (const auto& url : urls) {
    VLOG(1) << "Connecting to " << url;
    std::unique_ptr<base::Value> text{challenge_content.DeepCopy()};
    chromeos::http::PostJson(url, std::move(text), {}, transport_,
                             base::Bind(&Group::HandleLeaderChallengeResponse,
                                        per_state_factory_.GetWeakPtr()),
                             base::Bind(&IgnoreHttpFailure));
  }
}

void Group::HandleLeaderChallengeResponse(chromeos::http::RequestID request_id,
                                          scoped_ptr<Response> response) {
  chromeos::ErrorPtr error;
  std::unique_ptr<base::DictionaryValue> json_resp =
      chromeos::http::ParseJsonResponse(response.get(), nullptr, &error);
  if (!json_resp) {
    return;
  }

  VLOG(1) << "Got leadership response";

  std::string leader;
  std::string id;
  if (!json_resp->GetString(http_api::kChallengeLeaderKey, &leader) ||
      !json_resp->GetString(http_api::kChallengeIdKey, &id)) {
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

void Group::AnnounceLeadership() {
  for (const auto& peer_id : peers_) {
    SendLeaderAnnouncement(peer_id);
  }
}

void Group::SendLeaderAnnouncement(const std::string& peer_id) {
  std::vector<std::string> urls;
  if (!BuildApiUrls(kApiVerbAnnounce, peer_id, &urls)) {
    return;
  }
  base::DictionaryValue announcement_content;
  announcement_content.SetString(http_api::kAnnounceGroupKey, guid_);
  announcement_content.SetString(http_api::kAnnounceLeaderIdKey,
                                 delegate_->GetUUID());
  announcement_content.SetInteger(http_api::kAnnounceScoreKey, score_);
  for (const auto& url : urls) {
    VLOG(1) << "Connecting to " << url;
    std::unique_ptr<base::Value> text{announcement_content.DeepCopy()};
    chromeos::http::PostJson(url, std::move(text), {}, transport_,
                             base::Bind(&IgnoreHttpSuccess),
                             base::Bind(&IgnoreHttpFailure));
  }
}

void Group::ReplaceTimersWithMocksForTest(
    std::unique_ptr<base::Timer> wanderer_timer,
    std::unique_ptr<base::Timer> heartbeat_timer) {
  wanderer_timer_ = std::move(wanderer_timer);
  heartbeat_timer_ = std::move(heartbeat_timer);
}

}  // namespace leaderd
