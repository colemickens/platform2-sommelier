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
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void RemoveGroup(const std::string& group) = 0;
    virtual const std::string& GetUUID() const = 0;
    virtual std::vector<std::tuple<std::vector<uint8_t>, uint16_t>> GetIPInfo(
        const std::string& peer_uuid) const = 0;
  };

  using CompletionAction =
      chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;
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

  void ChallengeLeader(const std::string& uuid, int score, std::string* leader,
                       std::string* my_uuid);

  void AddPeer(const std::string& uuid);
  void RemovePeer(const std::string& uuid);
  void ClearPeers();

 private:
  void Reelect();
  void OnDBusServiceDeath();
  void RemoveSoon();
  void RemoveNow();

  bool IsScoreGreater(int score, const std::string& guid) const;
  void SetRole(State state, const std::string& leader);

  const std::string guid_;
  const dbus::ObjectPath object_path_;

  // A set of UUIDs of the peers advertising this group.
  std::set<std::string> peers_;
  Delegate* delegate_;
  State state_{State::WANDERER};
  int score_{0};
  std::string leader_;

  org::chromium::leaderd::GroupAdaptor dbus_adaptor_{this};
  chromeos::dbus_utils::DBusObject dbus_object_;
  std::unique_ptr<chromeos::dbus_utils::DBusServiceWatcher> service_watcher_;

  base::WeakPtrFactory<Group> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Group);
};

}  // namespace leaderd

#endif  // LEADERD_GROUP_H_
