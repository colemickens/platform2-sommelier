// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile.h"

#include <string>

#include <base/logging.h>
#include <base/stl_util-inl.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/property_accessor.h"
#include "shill/shill_event.h"

using std::string;

namespace shill {
Profile::Profile(ControlInterface *control_interface)
    : adaptor_(control_interface->CreateProfileAdaptor(this)) {
}

Profile::~Profile() {}

bool Profile::SetBoolProperty(const string& name, bool value, Error *error) {
  VLOG(2) << "Setting " << name << " as a bool.";
  bool set = (ContainsKey(bool_properties_, name) &&
              bool_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W bool.");
  return set;
}

bool Profile::SetStringProperty(const string& name,
                                const string& value,
                                Error *error) {
  VLOG(2) << "Setting " << name << " as a string.";
  bool set = (ContainsKey(string_properties_, name) &&
              string_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W string.");
  return set;
}

}  // namespace shill
