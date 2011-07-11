// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ephemeral_profile.h"

#include <string>
#include <map>

#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/entry.h"
#include "shill/manager.h"

using std::map;
using std::string;

namespace shill {

EphemeralProfile::EphemeralProfile(ControlInterface *control_interface,
                                   GLib *glib,
                                   Manager *manager)
    : Profile(control_interface, glib),
      manager_(manager) {
}

EphemeralProfile::~EphemeralProfile() {}

bool EphemeralProfile::MoveToActiveProfile(const std::string &entry_name) {
  map<string, EntryRefPtr>::iterator it = entries_.find(entry_name);
  if (it == entries_.end())
    return false;
  manager_->ActiveProfile()->AdoptEntry(entry_name, it->second);
  entries_.erase(it);
  return true;
}

}  // namespace shill
