//
// Copyright (C) 2014 The Android Open Source Project
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

#ifndef SHILL_DBUS_NAME_WATCHER_H_
#define SHILL_DBUS_NAME_WATCHER_H_

#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

namespace shill {

class DBusManager;

class DBusNameWatcher : public base::SupportsWeakPtr<DBusNameWatcher> {
 public:
  typedef base::Callback<void(const std::string& name,
                              const std::string& owner)> NameAppearedCallback;
  typedef base::Callback<void(const std::string& name)> NameVanishedCallback;

  // Constructs a watcher to monitor a given DBus service |name|. When the
  // service appears, |name_appeared_callback| is invoked if non-null. When the
  // service vanishes, |name_vanished_callback| is invoked if non-null. At
  // construction, it registers to a DBus manager |dbus_manager| in order to
  // receive notifications when |name| appears on or vanishes from DBus. At
  // desctruction, it dereigisters from |dbus_manager|.
  DBusNameWatcher(DBusManager* dbus_manager,
                  const std::string& name,
                  const NameAppearedCallback& name_appeared_callback,
                  const NameVanishedCallback& name_vanished_callback);
  ~DBusNameWatcher();

  // Called by |dbus_manager_| when |name_| appears on or vanishes from DBus.
  // |name_appeared_callback_| or |name_vanished_callback_| is invoked
  // accordingly if non-null.
  void OnNameOwnerChanged(const std::string& name) const;

  const std::string& name() const { return name_; }

 private:
  base::WeakPtr<DBusManager> dbus_manager_;
  std::string name_;
  NameAppearedCallback name_appeared_callback_;
  NameVanishedCallback name_vanished_callback_;

  DISALLOW_COPY_AND_ASSIGN(DBusNameWatcher);
};

}  // namespace shill

#endif  // SHILL_DBUS_NAME_WATCHER_H_
