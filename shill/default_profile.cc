// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/default_profile.h"

#include <vector>

#include <base/file_path.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/manager.h"
#include "shill/portal_detector.h"
#include "shill/store_interface.h"

using std::vector;

namespace shill {
// static
const char DefaultProfile::kDefaultId[] = "default";
// static
const char DefaultProfile::kStorageId[] = "global";
// static
const char DefaultProfile::kStorageCheckPortalList[] = "CheckPortalList";
// static
const char DefaultProfile::kStorageHostName[] = "HostName";
// static
const char DefaultProfile::kStorageName[] = "Name";
// static
const char DefaultProfile::kStorageOfflineMode[] = "OfflineMode";
// static
const char DefaultProfile::kStoragePortalURL[] = "PortalURL";
// static
const char DefaultProfile::kStoragePortalCheckInterval[] =
    "PortalCheckInterval";

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
  store->RegisterConstInt32(shill::kPortalCheckIntervalProperty,
                            &manager_props.portal_check_interval_seconds);
}

DefaultProfile::~DefaultProfile() {}

bool DefaultProfile::LoadManagerProperties(Manager::Properties *manager_props) {
  storage()->GetString(kStorageId, kStorageHostName, &manager_props->host_name);
  storage()->GetBool(kStorageId, kStorageOfflineMode,
                     &manager_props->offline_mode);
  storage()->GetString(kStorageId,
                       kStorageCheckPortalList,
                       &manager_props->check_portal_list);
  if (!storage()->GetString(kStorageId, kStoragePortalURL,
                            &manager_props->portal_url)) {
    manager_props->portal_url = PortalDetector::kDefaultURL;
  }
  std::string check_interval;
  if (!storage()->GetString(kStorageId, kStoragePortalCheckInterval,
                            &check_interval) ||
      !base::StringToInt(check_interval,
                         &manager_props->portal_check_interval_seconds)) {
    manager_props->portal_check_interval_seconds =
        PortalDetector::kDefaultCheckIntervalSeconds;
  }
  return true;
}

bool DefaultProfile::Save() {
  storage()->SetString(kStorageId, kStorageHostName, props_.host_name);
  storage()->SetString(kStorageId, kStorageName, GetFriendlyName());
  storage()->SetBool(kStorageId, kStorageOfflineMode, props_.offline_mode);
  storage()->SetString(kStorageId,
                       kStorageCheckPortalList,
                       props_.check_portal_list);
  storage()->SetString(kStorageId,
                       kStoragePortalURL,
                       props_.portal_url);
  storage()->SetString(kStorageId,
                       kStoragePortalCheckInterval,
                       base::IntToString(props_.portal_check_interval_seconds));
  vector<DeviceRefPtr>::iterator it;
  for (it = manager()->devices_begin(); it != manager()->devices_end(); ++it) {
    if (!(*it)->Save(storage())) {
      LOG(ERROR) << "Could not save " << (*it)->UniqueName();
      return false;
    }
  }
  return Profile::Save();
}

bool DefaultProfile::GetStoragePath(FilePath *path) {
  *path = storage_path_.Append(base::StringPrintf("%s.profile", kDefaultId));
  return true;
}

}  // namespace shill
