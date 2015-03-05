// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_MANAGER_H_
#define LEADERD_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <base/macros.h>
#include <chromeos/dbus/async_event_sequencer.h>

#include "leaderd/group.h"
#include "leaderd/org.chromium.leaderd.Manager.h"
#include "leaderd/peerd_client.h"
#include "leaderd/webserver_client.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

namespace leaderd {

// Manages global state of leaderd.
class Manager : public org::chromium::leaderd::ManagerInterface,
                public WebServerClient::Delegate,
                public PeerdClient::Delegate,
                public Group::Delegate {
 public:
  Manager(const scoped_refptr<dbus::Bus>& bus,
          chromeos::dbus_utils::ExportedObjectManager* object_manager,
          std::unique_ptr<PeerdClient> peerd_client);
  ~Manager();

  void RegisterAsync(chromeos::dbus_utils::AsyncEventSequencer* sequencer);

  // Group::Delegate overrides.
  void RemoveGroup(const std::string& group) override;
  const std::string& GetUUID() const override;
  std::vector<std::tuple<std::vector<uint8_t>, uint16_t>> GetIPInfo(
      const std::string& peer_uuid) const override;

  // WebServerClient::Delegate overrides.
  void SetWebServerPort(uint16_t port) override;
  bool ChallengeLeader(const std::string& in_uuid, const std::string& in_guid,
                       int32_t in_score, std::string* out_leader,
                       std::string* out_my_uuid) override;

  // DBus handlers.
  bool JoinGroup(chromeos::ErrorPtr* error, dbus::Message* message,
                 const std::string& group_id,
                 const std::map<std::string, chromeos::Any>& options,
                 dbus::ObjectPath* group_path) override;
  std::string Ping() override;

  // PeerdClient::Delegate overrides.
  void OnPeerdAvailable() override;
  void OnPeerdDeath() override;
  void OnSelfIdChanged(const std::string& uuid) override;
  void OnPeerGroupsChanged(const std::string& uuid,
                           const std::set<std::string>& groups) override;

 private:
  friend class ManagerTest;
  void PublishService();

  scoped_refptr<dbus::Bus> bus_;
  org::chromium::leaderd::ManagerAdaptor dbus_adaptor_{this};
  chromeos::dbus_utils::DBusObject dbus_object_;
  std::string uuid_;
  std::unique_ptr<PeerdClient> peerd_client_;
  // A mapping of group UUID to Group.
  std::map<std::string, std::unique_ptr<Group>> groups_;
  size_t last_group_dbus_id_{0};
  uint16_t web_port_;

  base::WeakPtrFactory<Manager> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace leaderd

#endif  // LEADERD_MANAGER_H_
