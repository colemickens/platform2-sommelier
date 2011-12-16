// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile.h"

#include <set>
#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/logging.h>
#include <base/stl_util-inl.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/key_file_store.h"
#include "shill/manager.h"
#include "shill/property_accessor.h"
#include "shill/service.h"
#include "shill/store_interface.h"

using std::set;
using std::string;
using std::vector;

namespace shill {

Profile::Profile(ControlInterface *control_interface,
                 Manager *manager,
                 const Identifier &name,
                 const string &user_storage_format,
                 bool connect_to_rpc)
    : manager_(manager),
      name_(name),
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

bool Profile::InitStorage(GLib *glib, InitStorageOption storage_option,
                          Error *error) {
  FilePath final_path;
  if (!GetStoragePath(&final_path)) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
        base::StringPrintf("Could not set up profile storage for %s:%s",
                           name_.user.c_str(), name_.identifier.c_str()));
    return false;
  }
  scoped_ptr<KeyFileStore> storage(new KeyFileStore(glib));
  storage->set_path(final_path);
  bool already_exists = storage->IsNonEmpty();
  if (!already_exists && storage_option != kCreateNew &&
      storage_option != kCreateOrOpenExisting) {
    Error::PopulateAndLog(error, Error::kNotFound,
        base::StringPrintf("Profile storage for %s:%s does not already exist",
                           name_.user.c_str(), name_.identifier.c_str()));
    return false;
  } else if (already_exists && storage_option != kOpenExisting &&
             storage_option != kCreateOrOpenExisting) {
    Error::PopulateAndLog(error, Error::kAlreadyExists,
        base::StringPrintf("Profile storage for %s:%s already exists",
                           name_.user.c_str(), name_.identifier.c_str()));
    return false;
  }
  if (!storage->Open()) {
    Error::PopulateAndLog(error, Error::kInternalError,
        base::StringPrintf("Could not open profile storage for %s:%s",
                           name_.user.c_str(), name_.identifier.c_str()));
    return false;
  }
  if (!already_exists) {
    // Add a descriptive header to the profile so even if nothing is stored
    // to it, it still has some content.  Completely empty keyfiles are not
    // valid for reading.
    storage->SetHeader(
        base::StringPrintf("Profile %s:%s", name_.user.c_str(),
                           name_.identifier.c_str()));
  }
  set_storage(storage.release());
  return true;
}

string Profile::GetFriendlyName() {
  return (name_.user.empty() ? "" : name_.user + "/") + name_.identifier;
}

string Profile::GetRpcIdentifier() {
  if (!adaptor_.get()) {
    // NB: This condition happens in unit tests.
    return string();
  }
  return adaptor_->GetRpcIdentifier();
}

void Profile::set_storage(StoreInterface *storage) {
  storage_.reset(storage);
}

bool Profile::AdoptService(const ServiceRefPtr &service) {
  if (storage_->ContainsGroup(service->GetStorageIdentifier()))
    return false;
  service->set_profile(this);
  return service->Save(storage_.get()) && storage_->Flush();
}

bool Profile::AbandonService(const ServiceRefPtr &service) {
  if (service->profile() == this)
    service->set_profile(NULL);
  return storage_->DeleteGroup(service->GetStorageIdentifier()) &&
      storage_->Flush();
}

bool Profile::UpdateService(const ServiceRefPtr &service) {
  return service->Save(storage_.get()) && storage_->Flush();
}

bool Profile::ConfigureService(const ServiceRefPtr &service) {
  if (!ContainsService(service))
    return false;
  service->set_profile(this);
  return service->Load(storage_.get());
}

bool Profile::ConfigureDevice(const DeviceRefPtr &device) {
  return device->Load(storage_.get());
}

bool Profile::ContainsService(const ServiceConstRefPtr &service) {
  return service->IsLoadableFrom(storage_.get());
}

bool Profile::IsValidIdentifierToken(const string &token) {
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

bool Profile::MatchesIdentifier(const Identifier &name) const {
  return name.user == name_.user && name.identifier == name_.identifier;
}

bool Profile::Save() {
  return storage_->Flush();
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

vector<string> Profile::EnumerateAvailableServices(Error *error) {
  // TOOD(quiche): This list should be based on the information we
  // have about services, rather than the Manager's service
  // list. (crosbug.com/23702)
  return manager_->EnumerateAvailableServices(error);
}

vector<string> Profile::EnumerateEntries(Error */*error*/) {
  // TODO(someone): Determine if we care about this wasteful copying; consider
  // making GetGroups return a vector.
  set<string> groups(storage_->GetGroups());
  return vector<string>(groups.begin(), groups.end());
}

void Profile::HelpRegisterDerivedStrings(
    const string &name,
    Strings(Profile::*get)(Error *),
    void(Profile::*set)(const Strings&, Error *)) {
  store_.RegisterDerivedStrings(
      name,
      StringsAccessor(new CustomAccessor<Profile, Strings>(this, get, set)));
}

}  // namespace shill
