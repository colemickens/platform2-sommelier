// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile.h"

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/stl_util-inl.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/manager.h"
#include "shill/property_accessor.h"
#include "shill/service.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

Profile::Profile(ControlInterface *control_interface,
                 GLib *glib,
                 Manager *manager,
                 const Identifier &name,
                 const string &user_storage_format,
                 bool connect_to_rpc)
    : manager_(manager),
      name_(name),
      storage_(glib),
      storage_format_(user_storage_format) {
  if (connect_to_rpc)
    adaptor_.reset(control_interface->CreateProfileAdaptor(this));

  // flimflam::kCheckPortalListProperty: Registered in DefaultProfile
  // flimflam::kCountryProperty: Registered in DefaultProfile
  store_.RegisterConstString(flimflam::kNameProperty, &name_.identifier);

  // flimflam::kOfflineModeProperty: Registered in DefaultProfile
  // flimflam::kPortalURLProperty: Registered in DefaultProfile

  HelpRegisterDerivedStrings(flimflam::kServicesProperty,
                             &Profile::EnumerateAvailableServices,
                             NULL);
  HelpRegisterDerivedStrings(flimflam::kEntriesProperty,
                             &Profile::EnumerateEntries,
                             NULL);
}

Profile::~Profile() {}

string Profile::GetFriendlyName() {
  return (name_.user.empty() ? "" : name_.user + "/") + name_.identifier;
}

string Profile::GetRpcIdentifier() {
  return adaptor_->GetRpcIdentifier();
}

bool Profile::AdoptService(const ServiceRefPtr &service) {
  if (ContainsKey(services_, service->UniqueName()))
    return false;
  service->set_profile(this);
  services_[service->UniqueName()] = service;
  return true;
}

bool Profile::AbandonService(const string &name) {
  map<string, ServiceRefPtr>::iterator to_abandon = services_.find(name);
  if (to_abandon != services_.end()) {
    services_.erase(to_abandon);
    return true;
  }
  return false;
}

bool Profile::DemoteService(const string &name) {
  map<string, ServiceRefPtr>::iterator to_demote = services_.find(name);
  if (to_demote == services_.end())
    return false;
  return true;  // TODO(cmasone): mark |to_demote| as inactive or something.
}

bool Profile::MergeService(const ServiceRefPtr &service) {
  map<string, ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (Mergeable(it->second, service))
      return true;  // TODO(cmasone): Perform merge.
  }
  return false;
}

ServiceRefPtr Profile::FindService(const std::string& name) {
  if (ContainsKey(services_, name))
    return services_[name];
  return NULL;
}

void Profile::Finalize() {
  // TODO(cmasone): Flush all of |services_| to disk if needed.
  services_.clear();
}

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

bool Profile::GetStoragePath(FilePath *path) {
  if (name_.user.empty()) {
    LOG(ERROR) << "Non-default profiles cannot be stored globally.";
    return false;
  }
  FilePath dir(base::StringPrintf(storage_format_.c_str(), name_.user.c_str()));
  // TODO(petkov): Validate the directory permissions, etc.
  *path = dir.Append(base::StringPrintf("%s.profile",
                                        name_.identifier.c_str()));
  return true;
}

vector<string> Profile::EnumerateAvailableServices() {
  return manager_->EnumerateAvailableServices();
}

vector<string> Profile::EnumerateEntries() {
  vector<string> rpc_ids;
  map<string, ServiceRefPtr>::const_iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    rpc_ids.push_back(it->second->GetRpcIdentifier());
  }
  return rpc_ids;
}

void Profile::HelpRegisterDerivedStrings(const string &name,
                                     Strings(Profile::*get)(void),
                                     bool(Profile::*set)(const Strings&)) {
  store_.RegisterDerivedStrings(
      name,
      StringsAccessor(new CustomAccessor<Profile, Strings>(this, get, set)));
}

}  // namespace shill
