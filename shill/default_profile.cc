// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/default_profile.h"

#include <base/file_path.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/manager.h"

namespace shill {
const char DefaultProfile::kDefaultId[] = "default";

DefaultProfile::DefaultProfile(ControlInterface *control,
                               GLib *glib,
                               Manager *manager,
                               const FilePath &storage_path,
                               const Manager::Properties &manager_props)
    : Profile(control, glib, manager, Identifier(kDefaultId), "", true),
      storage_path_(storage_path) {
  store_.RegisterConstString(flimflam::kCheckPortalListProperty,
                             &manager_props.check_portal_list);
  store_.RegisterConstString(flimflam::kCountryProperty,
                             &manager_props.country);
  store_.RegisterConstBool(flimflam::kOfflineModeProperty,
                           &manager_props.offline_mode);
  store_.RegisterConstString(flimflam::kPortalURLProperty,
                             &manager_props.portal_url);
}

DefaultProfile::~DefaultProfile() {}

bool DefaultProfile::GetStoragePath(FilePath *path) {
  *path = storage_path_.Append(base::StringPrintf("%s.profile", kDefaultId));
  return true;
}

}  // namespace shill
