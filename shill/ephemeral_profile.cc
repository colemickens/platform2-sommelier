// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ephemeral_profile.h"

#include <string>
#include <map>

#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
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

bool EphemeralProfile::MoveToActiveProfile(const std::string &name) {
  map<string, ServiceRefPtr>::iterator it = services_.find(name);
  if (it == services_.end())
    return false;
  manager_->ActiveProfile()->AdoptService(name, it->second);
  services_.erase(it);
  return true;
}

}  // namespace shill
