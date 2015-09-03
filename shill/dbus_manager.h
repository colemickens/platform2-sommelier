//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DBUS_MANAGER_H_
#define SHILL_DBUS_MANAGER_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/dbus_name_watcher.h"

namespace shill {

class ControlInterface;
class DBusServiceProxyInterface;
class Error;

class DBusManager : public base::SupportsWeakPtr<DBusManager> {
 public:
  explicit DBusManager(ControlInterface* control_interface);
  virtual ~DBusManager();

  void Start();
  void Stop();

  // Creates and registers a watcher for DBus service |name|. When the service
  // appears, |name_appeared_callback| is invoked if non-null. When the service
  // vanishes, |name_vanished_callback| is invoked if non-null.
  // |name_appeared_callback| or |name_vanished_callback| will be notified once
  // asynchronously if the service has or doesn't have an owner, respectively,
  // when this method is invoked. The returned watcher should be managed by the
  // caller and may outlive this DBus manager. The watcher holds a weak pointer
  // to this DBus manager.  When it is destructed, it automatically calls
  // RemoveNameWatcher() to deregister and remove itself from this DBus
  // manager.
  virtual DBusNameWatcher* CreateNameWatcher(
      const std::string& name,
      const DBusNameWatcher::NameAppearedCallback& name_appeared_callback,
      const DBusNameWatcher::NameVanishedCallback& name_vanished_callback);

  // Deregisters and removes the watcher such that it stops monitoring the
  // associated DBus service name.
  virtual void RemoveNameWatcher(DBusNameWatcher* name_watcher);

 private:
  friend class DBusManagerTest;
  friend class ModemInfoTest;
  friend class ModemManagerTest;
  friend class PowerManagerTest;
  friend class WiFiObjectTest;
  friend class WiMaxProviderTest;
  FRIEND_TEST(DBusManagerTest, NameWatchers);
  FRIEND_TEST(WiMaxProviderTest, StartStop);
  FRIEND_TEST(ModemManagerCoreTest, OnAppearVanish);

  void OnNameOwnerChanged(const std::string& name,
                          const std::string& old_owner,
                          const std::string& new_owner);

  void OnGetNameOwnerComplete(
      const base::WeakPtr<DBusNameWatcher>& name_watcher,
      const std::string& unique_name,
      const Error& error);

  ControlInterface* control_interface_;

  std::unique_ptr<DBusServiceProxyInterface> proxy_;

  std::map<std::string, std::list<DBusNameWatcher*>> name_watchers_;

  DISALLOW_COPY_AND_ASSIGN(DBusManager);
};

}  // namespace shill

#endif  // SHILL_DBUS_MANAGER_H_
