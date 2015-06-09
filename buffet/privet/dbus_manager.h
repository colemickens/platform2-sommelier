// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_PRIVET_DBUS_MANAGER_H_
#define BUFFET_PRIVET_DBUS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/object_path.h>

#include "buffet/privet/org.chromium.privetd.Manager.h"
#include "buffet/privet/wifi_bootstrap_manager.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

namespace privetd {

class CloudDelegate;
class SecurityManager;
enum class PairingType;

// Exposes most of the privetd DBus interface.
class DBusManager : public org::chromium::privetd::ManagerInterface {
 public:
  using CompletionAction =
      chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;

  DBusManager(chromeos::dbus_utils::ExportedObjectManager* object_manager,
              WifiBootstrapManager* wifi_bootstrap_manager,
              CloudDelegate* cloud_delegate,
              SecurityManager* security_manager);
  ~DBusManager() override = default;
  void RegisterAsync(const CompletionAction& on_done);

  // DBus handlers
  bool EnableWiFiBootstrapping(
      chromeos::ErrorPtr* error,
      const dbus::ObjectPath& in_listener_path,
      const chromeos::VariantDictionary& in_options) override;
  bool DisableWiFiBootstrapping(chromeos::ErrorPtr* error) override;
  bool EnableGCDBootstrapping(
      chromeos::ErrorPtr* error,
      const dbus::ObjectPath& in_listener_path,
      const chromeos::VariantDictionary& in_options) override;
  bool DisableGCDBootstrapping(chromeos::ErrorPtr* error) override;
  std::string Ping() override;

 private:
  void UpdateWiFiBootstrapState(WifiBootstrapManager::State state);
  void OnPairingStart(const std::string& session_id,
                      PairingType pairing_type,
                      const std::vector<uint8_t>& code);
  void OnPairingEnd(const std::string& session_id);

  org::chromium::privetd::ManagerAdaptor dbus_adaptor_{this};
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  base::WeakPtrFactory<DBusManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DBusManager);
};

}  // namespace privetd

#endif  // BUFFET_PRIVET_DBUS_MANAGER_H_
