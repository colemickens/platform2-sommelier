// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile.h"

#include <string>

#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/property_accessor.h"

using std::string;

namespace shill {

const char Profile::kGlobalStorageDir[] = "/var/cache/flimflam";
const char Profile::kUserStorageDirFormat[] = "/home/%s/user/flimflam";

Profile::Profile(ControlInterface *control_interface,
                 GLib *glib)
    : adaptor_(control_interface->CreateProfileAdaptor(this)),
      storage_(glib) {
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

bool Profile::IsValidIdentifierToken(const std::string &token) {
  if (token.empty()) {
    return false;
  }
  for (string::const_iterator it = token.begin(); it != token.end(); ++it) {
    if (!IsAsciiAlpha(*it) && !IsAsciiDigit(*it)) {
      return false;
    }
  }
  return true;
}

bool Profile::ParseIdentifier(const string &raw, Identifier *parsed) {
  if (raw.empty()) {
    return false;
  }
  if (raw[0] == '~') {
    // Format: "~user/identifier".
    size_t slash = raw.find('/');
    if (slash == string::npos) {
      return false;
    }
    string user(raw.begin() + 1, raw.begin() + slash);
    string identifier(raw.begin() + slash + 1, raw.end());
    if (!IsValidIdentifierToken(user) || !IsValidIdentifierToken(identifier)) {
      return false;
    }
    parsed->user = user;
    parsed->identifier = identifier;
    return true;
  }

  // Format: "identifier".
  if (!IsValidIdentifierToken(raw)) {
    return false;
  }
  parsed->user = "";
  parsed->identifier = raw;
  return true;
}

string Profile::GetRpcPath(const Identifier &identifier) {
  string user = identifier.user.empty() ? "" : identifier.user + "/";
  return "/profile/" + user + identifier.identifier;
}

bool Profile::GetStoragePath(const Identifier &identifier, FilePath *path) {
  FilePath dir(
      identifier.user.empty() ?
      kGlobalStorageDir :
      base::StringPrintf(kUserStorageDirFormat, identifier.user.c_str()));
  // TODO(petkov): Validate the directory permissions, etc.
  *path = dir.Append(base::StringPrintf("%s.profile",
                                        identifier.identifier.c_str()));
  return true;
}

}  // namespace shill
