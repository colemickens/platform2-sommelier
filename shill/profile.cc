// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile.h"

#include <string>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/property_accessor.h"

using std::string;

namespace shill {
Profile::Profile(ControlInterface *control_interface)
    : adaptor_(control_interface->CreateProfileAdaptor(this)) {
  // flimflam::kCheckPortalListProperty: Registered in DefaultProfile
  // flimflam::kCountryProperty: Registered in DefaultProfile
  store_.RegisterConstString(flimflam::kNameProperty, &name_);

  // flimflam::kOfflineModeProperty: Registered in DefaultProfile
  // flimflam::kPortalURLProperty: Registered in DefaultProfile

  // TODO(cmasone): Implement these once we figure out where Profiles fit.
  // HelpRegisterDerivedStrings(flimflam::kServicesProperty,
  //                            &Manager::EnumerateAvailableServices,
  //                            NULL);
  // HelpRegisterDerivedStrings(flimflam::kEntriesProperty,
  //                            &Manager::EnumerateEntries,
  //                            NULL);
}

Profile::~Profile() {}

}  // namespace shill
