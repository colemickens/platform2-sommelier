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
#include "shill/store_interface.h"

namespace shill {
// static
const char DefaultProfile::kDefaultId[] = "default";
// static
const char DefaultProfile::kStorageId[] = "global";
// static
const char DefaultProfile::kStorageCheckPortalList[] = "CheckPortalList";
// static
const char DefaultProfile::kStorageName[] = "Name";
// static
const char DefaultProfile::kStorageOfflineMode[] = "OfflineMode";

DefaultProfile::DefaultProfile(ControlInterface *control,
                               Manager *manager,
                               const FilePath &storage_path,
                               const Manager::Properties &manager_props)
    : Profile(control, manager, Identifier(kDefaultId), "", true),
      storage_path_(storage_path),
      props_(manager_props) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kCheckPortalListProperty,
                             &manager_props.check_portal_list);
  store->RegisterConstString(flimflam::kCountryProperty,
                             &manager_props.country);
  store->RegisterConstBool(flimflam::kOfflineModeProperty,
                           &manager_props.offline_mode);
  store->RegisterConstString(flimflam::kPortalURLProperty,
                             &manager_props.portal_url);
}

DefaultProfile::~DefaultProfile() {}

bool DefaultProfile::Save(StoreInterface *storage) {
  storage->SetString(kStorageId, kStorageName, GetFriendlyName());
  storage->SetBool(kStorageId, kStorageOfflineMode, props_.offline_mode);
  storage->SetString(kStorageId,
                     kStorageCheckPortalList,
                     props_.check_portal_list);
  return Profile::Save(storage);
}

bool DefaultProfile::GetStoragePath(FilePath *path) {
  *path = storage_path_.Append(base::StringPrintf("%s.profile", kDefaultId));
  return true;
}

}  // namespace shill
