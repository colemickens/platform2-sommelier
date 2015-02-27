// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_MANAGER_H_
#define LEADERD_MANAGER_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <chromeos/dbus/async_event_sequencer.h>

#include "leaderd/org.chromium.leaderd.Manager.h"
#include "leaderd/peerd_client.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

namespace leaderd {

// Manages global state of leaderd.
class Manager : public org::chromium::leaderd::ManagerInterface,
                public PeerdClient::Delegate {
 public:
  Manager(const scoped_refptr<dbus::Bus>& bus,
          chromeos::dbus_utils::ExportedObjectManager* object_manager,
          std::unique_ptr<PeerdClient> peerd_client);
  ~Manager();

  void RegisterAsync(chromeos::dbus_utils::AsyncEventSequencer* sequencer);

  // DBus handlers.
  bool JoinGroup(chromeos::ErrorPtr* error, dbus::Message* message,
                 const std::string& group_id,
                 const std::map<std::string, chromeos::Any>& options,
                 dbus::ObjectPath* group_path) override;
  std::string Ping() override;

  // PeerdClient::Delegate overrides.
  void OnPeerdManagerAdded(
      org::chromium::peerd::ManagerProxyInterface* manager_proxy) override;
  void OnPeerdManagerRemoved(const dbus::ObjectPath& object_path) override;
  void OnPeerdPeerAdded(org::chromium::peerd::PeerProxyInterface* peer_proxy,
                        const dbus::ObjectPath& object_path) override;
  void OnPeerdPeerRemoved(const dbus::ObjectPath& object_path) override;
  void OnPeerdServiceAdded(
      org::chromium::peerd::ServiceProxyInterface* service_proxy,
      const dbus::ObjectPath& object_path) override;
  void OnPeerdServiceRemoved(const dbus::ObjectPath& object_path) override;
  void OnPeerdServiceChanged(
      org::chromium::peerd::ServiceProxyInterface* service_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& property) override;

 private:
  scoped_refptr<dbus::Bus> bus_;
  org::chromium::leaderd::ManagerAdaptor dbus_adaptor_{this};
  chromeos::dbus_utils::DBusObject dbus_object_;
  std::string uuid_;
  std::unique_ptr<PeerdClient> peerd_client_;

  base::WeakPtrFactory<Manager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace leaderd

#endif  // LEADERD_MANAGER_H_
