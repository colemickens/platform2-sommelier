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

#include "shill/dbus_name_watcher.h"

#include "shill/dbus_manager.h"

using std::string;

namespace shill {

DBusNameWatcher::DBusNameWatcher(
    DBusManager* dbus_manager,
    const string& name,
    const NameAppearedCallback& name_appeared_callback,
    const NameVanishedCallback& name_vanished_callback)
    : dbus_manager_(dbus_manager->AsWeakPtr()),
      name_(name),
      name_appeared_callback_(name_appeared_callback),
      name_vanished_callback_(name_vanished_callback) {}

DBusNameWatcher::~DBusNameWatcher() {
  if (dbus_manager_)
    dbus_manager_->RemoveNameWatcher(this);
}

void DBusNameWatcher::OnNameOwnerChanged(const string& owner) const {
  if (owner.empty()) {
    if (!name_vanished_callback_.is_null()) {
      name_vanished_callback_.Run(name_);
    }
  } else {
    if (!name_appeared_callback_.is_null()) {
      name_appeared_callback_.Run(name_, owner);
    }
  }
}

}  // namespace shill
