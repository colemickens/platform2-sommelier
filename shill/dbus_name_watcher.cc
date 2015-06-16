// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
