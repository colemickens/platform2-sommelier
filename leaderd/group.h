// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_GROUP_H_
#define LEADERD_GROUP_H_

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <base/macros.h>
#include <base/timer/timer.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/dbus_service_watcher.h>
#include <chromeos/http/http_transport.h>

#include "leaderd/org.chromium.leaderd.Group.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

namespace leaderd {

// Represents a single group advertisement.
class Group : public org::chromium::leaderd::GroupInterface {
 public:
  using IpInfo = std::vector<std::tuple<std::vector<uint8_t>, uint16_t>>;
  using CompletionAction =
      chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void RemoveGroup(const std::string& group) = 0;
    virtual const std::string& GetUUID() const = 0;
    virtual IpInfo GetIPInfo(const std::string& peer_id) const = 0;
  };

  enum class State { WANDERER, FOLLOWER, LEADER };

  Group(const std::string& guid, const scoped_refptr<dbus::Bus>& bus,
        chromeos::dbus_utils::ExportedObjectManager* object_manager,
        const dbus::ObjectPath& path, const std::string& dbus_connection_id,
        const std::set<std::string>& peer_list, Delegate* delegate);
  void RegisterAsync(const CompletionAction& completion_callback);
  const dbus::ObjectPath& GetObjectPath() const;

  // DBus handlers
  bool LeaveGroup(chromeos::ErrorPtr* error) override;
  bool SetScore(chromeos::ErrorPtr* error, int32_t in_score) override;
  bool PokeLeader(chromeos::ErrorPtr* error) override;

  // The manager informs us when we need to respond to a challenge from a peer.
  void HandleLeaderChallenge(const std::string& challenger_id,
                             int32_t challenger_score,
                             std::string* leader_id,
                             std::string* my_id);
  // The manager informs us of leadership announcements from our peers.
  bool HandleLeaderAnnouncement(const std::string& leader_id,
                                int32_t leader_score);

  // The manager informs us of changes in group membership.
  void AddPeer(const std::string& uuid);
  void RemovePeer(const std::string& uuid);
  void ClearPeers();

 private:
  friend class GroupTest;
  FRIEND_TEST(GroupTest, LeaderChallengeIsWellFormed);
  FRIEND_TEST(GroupTest, LeaderAnnouncementIsWellFormed);

  void Reelect();
  void OnDBusServiceDeath();
  void RemoveSoon();
  void RemoveNow();
  bool IsScoreGreater(int score, const std::string& guid) const;
  void SetRole(State state, const std::string& leader);
  void OnWandererTimeout();
  bool BuildApiUrls(const std::string& api_verb,
                    const std::string& peer_id,
                    std::vector<std::string>* urls) const;

  // These methods revolve around sending and handling responses to our
  // own leader challenges.
  void AskPeersForLeaderInfo();
  void SendLeaderChallenge(const std::string& peer_id);
  void HandleLeaderChallengeResponse(
      int request_id, scoped_ptr<chromeos::http::Response> response);
  // These methods let us announce our leadership.  We ignore results.
  void AnnounceLeadership();
  void SendLeaderAnnouncement(const std::string& peer_id);

  // Used in tests.
  void ReplaceTimersWithMocksForTest(
      std::unique_ptr<base::Timer> wanderer_timer,
      std::unique_ptr<base::Timer> heartbeat_timer);
  void ReplaceHTTPTransportForTest(
      std::shared_ptr<chromeos::http::Transport> transport) {
    transport_ = transport;
  }

  const std::string guid_;
  const dbus::ObjectPath object_path_;
  std::unique_ptr<base::Timer> wanderer_timer_{
      new base::OneShotTimer<Group>()};
  std::unique_ptr<base::Timer> heartbeat_timer_{
      new base::RepeatingTimer<Group>()};

  // A set of UUIDs of the peers advertising this group.
  std::set<std::string> peers_;
  Delegate* delegate_;
  State state_{State::WANDERER};
  int score_{0};
  std::string leader_;
  std::shared_ptr<chromeos::http::Transport> transport_;

  org::chromium::leaderd::GroupAdaptor dbus_adaptor_{this};
  chromeos::dbus_utils::DBusObject dbus_object_;
  std::unique_ptr<chromeos::dbus_utils::DBusServiceWatcher> service_watcher_;

  base::WeakPtrFactory<Group> lifetime_factory_{this};
  base::WeakPtrFactory<Group> per_state_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Group);
};

}  // namespace leaderd

#endif  // LEADERD_GROUP_H_
