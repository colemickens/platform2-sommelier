// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <time.h>
#include <stdio.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/stl_util-inl.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/default_profile.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/ephemeral_profile.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/key_file_store.h"
#include "shill/service_sorter.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/resolver.h"
#include "shill/service.h"
#include "shill/wifi.h"
#include "shill/wifi_service.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

// static
const char Manager::kManagerErrorNoDevice[] = "no wifi devices available";

Manager::Manager(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 GLib *glib,
                 const string &run_directory,
                 const string &storage_directory,
                 const string &user_storage_format)
  : run_path_(FilePath(run_directory)),
    storage_path_(FilePath(storage_directory)),
    user_storage_format_(user_storage_format),
    adaptor_(control_interface->CreateManagerAdaptor(this)),
    device_info_(control_interface, dispatcher, this),
    modem_info_(control_interface, dispatcher, this, glib),
    running_(false),
    connect_profiles_to_rpc_(true),
    ephemeral_profile_(new EphemeralProfile(control_interface, this)),
    control_interface_(control_interface),
    glib_(glib) {
  HelpRegisterDerivedString(flimflam::kActiveProfileProperty,
                            &Manager::GetActiveProfileName,
                            NULL);
  HelpRegisterDerivedStrings(flimflam::kAvailableTechnologiesProperty,
                             &Manager::AvailableTechnologies,
                             NULL);
  store_.RegisterString(flimflam::kCheckPortalListProperty,
                        &props_.check_portal_list);
  HelpRegisterDerivedStrings(flimflam::kConnectedTechnologiesProperty,
                             &Manager::ConnectedTechnologies,
                             NULL);
  store_.RegisterString(flimflam::kCountryProperty, &props_.country);
  HelpRegisterDerivedString(flimflam::kDefaultTechnologyProperty,
                            &Manager::DefaultTechnology,
                            NULL);
  HelpRegisterDerivedStrings(flimflam::kDevicesProperty,
                             &Manager::EnumerateDevices,
                             NULL);
  HelpRegisterDerivedStrings(flimflam::kEnabledTechnologiesProperty,
                             &Manager::EnabledTechnologies,
                             NULL);
  store_.RegisterBool(flimflam::kOfflineModeProperty, &props_.offline_mode);
  store_.RegisterString(flimflam::kPortalURLProperty, &props_.portal_url);
  HelpRegisterDerivedString(flimflam::kStateProperty,
                            &Manager::CalculateState,
                            NULL);
  HelpRegisterDerivedStrings(flimflam::kServicesProperty,
                             &Manager::EnumerateAvailableServices,
                             NULL);
  HelpRegisterDerivedStrings(flimflam::kServiceWatchListProperty,
                             &Manager::EnumerateWatchedServices,
                             NULL);

  // TODO(cmasone): Wire these up once we actually put in profile support.
  // known_properties_.push_back(flimflam::kProfilesProperty);
  VLOG(2) << "Manager initialized.";
}

Manager::~Manager() {
  profiles_.clear();
}

void Manager::AddDeviceToBlackList(const string &device_name) {
  device_info_.AddDeviceToBlackList(device_name);
}

void Manager::Start() {
  LOG(INFO) << "Manager started.";

  CHECK(file_util::CreateDirectory(run_path_)) << run_path_.value();
  Resolver::GetInstance()->set_path(run_path_.Append("resolv.conf"));

  CHECK(file_util::CreateDirectory(storage_path_)) << storage_path_.value();

  profiles_.push_back(new DefaultProfile(control_interface_,
                                         this,
                                         storage_path_,
                                         props_));
  CHECK(profiles_[0]->InitStorage(glib_, Profile::kCreateOrOpenExisting, NULL));

  running_ = true;
  adaptor_->UpdateRunning();
  device_info_.Start();
  modem_info_.Start();
}

void Manager::Stop() {
  running_ = false;
  // Persist profile, device, service information to disk.
  vector<ProfileRefPtr>::iterator it;
  for (it = profiles_.begin(); it != profiles_.end(); ++it) {
    (*it)->Save();
  }

  vector<ServiceRefPtr>::iterator services_it;
  for (services_it = services_.begin(); services_it != services_.end();
       ++services_it) {
    (*services_it)->Disconnect();
  }

  adaptor_->UpdateRunning();
  modem_info_.Stop();
  device_info_.Stop();
}

void Manager::CreateProfile(const string &name, Error *error) {
  Profile::Identifier ident;
  if (!Profile::ParseIdentifier(name, &ident)) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Invalid profile name " + name);
    return;
  }
  ProfileRefPtr profile(new Profile(control_interface_,
                                    this,
                                    ident,
                                    user_storage_format_,
                                    false));
  if (!profile->InitStorage(glib_, Profile::kCreateNew, error)) {
    return;
  }

  // Save profile data out, and then let the scoped pointer fall out of scope.
  if (!profile->Save()) {
    Error::PopulateAndLog(error, Error::kInternalError,
                          "Profile name " + name + " could not be saved");
    return;
  }
}

void Manager::PushProfile(const string &name, Error *error) {
  Profile::Identifier ident;
  if (!Profile::ParseIdentifier(name, &ident)) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Invalid profile name " + name);
    return;
  }

  for (vector<ProfileRefPtr>::const_iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    if ((*it)->MatchesIdentifier(ident)) {
      Error::PopulateAndLog(error, Error::kAlreadyExists,
                            "Profile name " + name + " is already on stack");
      return;
    }
  }

  if (ident.user.empty()) {
    // The manager will have only one machine-wide profile, and this is the
    // DefaultProfile.  This means no other profiles can be loaded that do
    // not have a user component.
    // TODO(pstew): This is all well and good, but WiFi autotests try to
    // creating a default profile (by a name other than "default") in order
    // to avoid leaving permanent side effects to devices under test.  This
    // whole thing will need to be reworked in order to allow that to happen,
    // or the autotests (or their expectations) will need to change.
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Cannot load non-default global profile " + name);
    return;
  }

  ProfileRefPtr profile(new Profile(control_interface_,
                                    this,
                                    ident,
                                    user_storage_format_,
                                    connect_profiles_to_rpc_));
  if (!profile->InitStorage(glib_, Profile::kOpenExisting, error)) {
    return;
  }

  profiles_.push_back(profile);

  // Offer each registered Service the opportunity to join this new Profile.
  for (vector<ServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    profile->ConfigureService(*it);
  }

  // Shop the Profile contents around to Devices which can create
  // non-visible services.
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    profile->ConfigureDevice(*it);
  }
}

void Manager::PopProfileInternal() {
  CHECK(!profiles_.empty());
  ProfileRefPtr active_profile = profiles_.back();
  profiles_.pop_back();
  vector<ServiceRefPtr>::iterator s_it;
  for (s_it = services_.begin(); s_it != services_.end(); ++s_it) {
    if ((*s_it)->profile().get() == active_profile.get()) {
      vector<ProfileRefPtr>::reverse_iterator p_it;
      for (p_it = profiles_.rbegin(); p_it != profiles_.rend(); ++p_it) {
        if ((*p_it)->ConfigureService(*s_it)) {
          break;
        }
      }
      if (p_it == profiles_.rend()) {
        ephemeral_profile_->AdoptService(*s_it);
        (*s_it)->Unload();
      }
    }
  }
}

void Manager::PopProfile(const string &name, Error *error) {
  Profile::Identifier ident;
  if (profiles_.empty()) {
    Error::PopulateAndLog(error, Error::kNotFound, "Profile stack is empty");
    return;
  }
  ProfileRefPtr active_profile = profiles_.back();
  if (!Profile::ParseIdentifier(name, &ident)) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Invalid profile name " + name);
    return;
  }
  if (!active_profile->MatchesIdentifier(ident)) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          name + " is not the active profile");
    return;
  }
  PopProfileInternal();
}

void Manager::PopAnyProfile(Error *error) {
  Profile::Identifier ident;
  if (profiles_.empty()) {
    Error::PopulateAndLog(error, Error::kNotFound, "Profile stack is empty");
    return;
  }
  PopProfileInternal();
}

const ProfileRefPtr &Manager::ActiveProfile() {
  DCHECK_NE(profiles_.size(), 0);
  return profiles_.back();
}

bool Manager::MoveServiceToProfile(const ServiceRefPtr &to_move,
                                   const ProfileRefPtr &destination) {
  const ProfileRefPtr from = to_move->profile();
  return destination->AdoptService(to_move) &&
      from->AbandonService(to_move);
}

void Manager::RegisterDevice(const DeviceRefPtr &to_manage) {
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_manage.get() == it->get())
      return;
  }
  devices_.push_back(to_manage);

  // We are applying device properties from the DefaultProfile, and adding
  // the union of hidden services in all loaded profiles to the device.
  for (vector<ProfileRefPtr>::iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    // Load device configuration, if any exists, as well as hidden services.
    (*it)->ConfigureDevice(to_manage);

    // Currently the only profile for which "Save" is implemented is the
    // DefaultProfile.  It iterates over all Devices and stores their state.
    // We perform the Save now in case the device we have just registered
    // is new and needs to be added to the stored DefaultProfile.
    (*it)->Save();
  }

  // In normal usage, running_ will always be true when we are here, however
  // unit tests sometimes do things in otherwise invalid states.
  if (running_ && to_manage->powered())
    to_manage->Start();

  Error error;
  adaptor_->EmitStringsChanged(flimflam::kAvailableTechnologiesProperty,
                               AvailableTechnologies(&error));
  adaptor_->EmitStringsChanged(flimflam::kEnabledTechnologiesProperty,
                               EnabledTechnologies(&error));
}

void Manager::DeregisterDevice(const DeviceRefPtr &to_forget) {
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_forget.get() == it->get()) {
      VLOG(2) << "Deregistered device: " << to_forget->UniqueName();
      to_forget->Stop();
      devices_.erase(it);
      Error error;
      adaptor_->EmitStringsChanged(flimflam::kAvailableTechnologiesProperty,
                                   AvailableTechnologies(&error));
      adaptor_->EmitStringsChanged(flimflam::kEnabledTechnologiesProperty,
                                   EnabledTechnologies(&error));
      return;
    }
  }
  VLOG(2) << __func__ << " unknown device: " << to_forget->UniqueName();
}

void Manager::RegisterService(const ServiceRefPtr &to_manage) {
  VLOG(2) << "In " << __func__ << "(): Registering service "
          << to_manage->UniqueName();

  bool configured = false;
  for (vector<ProfileRefPtr>::reverse_iterator it = profiles_.rbegin();
       !configured && it != profiles_.rend();
       ++it) {
    configured = (*it)->ConfigureService(to_manage);
  }

  // If not found, add it to the ephemeral profile
  if (!configured)
    ephemeral_profile_->AdoptService(to_manage);

  // Now add to OUR list.
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    CHECK(to_manage->UniqueName() != (*it)->UniqueName());
  }
  services_.push_back(to_manage);
  SortServices();
}

void Manager::DeregisterService(const ServiceRefPtr &to_forget) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_forget->UniqueName() == (*it)->UniqueName()) {
      services_.erase(it);
      SortServices();
      return;
    }
  }
}

void Manager::UpdateService(const ServiceRefPtr &to_update) {
  CHECK(to_update);
  LOG(INFO) << "Service " << to_update->UniqueName() << " updated;"
            << " state: " << Service::ConnectStateToString(to_update->state())
            << " failure: "
            << Service::ConnectFailureToString(to_update->failure());
  LOG(INFO) << "IsConnected(): " << to_update->IsConnected();
  if (to_update->IsConnected())
    to_update->MakeFavorite();
  SortServices();
}

void Manager::FilterByTechnology(Technology::Identifier tech,
                                 vector<DeviceRefPtr> *found) {
  CHECK(found);
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if ((*it)->TechnologyIs(tech))
      found->push_back(*it);
  }
}

ServiceRefPtr Manager::FindService(const string& name) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (name == (*it)->UniqueName())
      return *it;
  }
  return NULL;
}

void Manager::HelpRegisterDerivedString(
    const string &name,
    string(Manager::*get)(Error *),
    void(Manager::*set)(const string&, Error *)) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Manager, string>(this, get, set)));
}

void Manager::HelpRegisterDerivedStrings(
    const string &name,
    Strings(Manager::*get)(Error *),
    void(Manager::*set)(const Strings &, Error *)) {
  store_.RegisterDerivedStrings(
      name,
      StringsAccessor(new CustomAccessor<Manager, Strings>(this, get, set)));
}

void Manager::SortServices() {
  sort(services_.begin(), services_.end(), ServiceSorter(technology_order_));

  vector<string> service_paths;
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if ((*it)->IsVisible()) {
      service_paths.push_back((*it)->GetRpcIdentifier());
    }
  }
  adaptor_->EmitRpcIdentifierArrayChanged(flimflam::kServicesProperty,
                                          service_paths);

  Error error;
  adaptor_->EmitStringsChanged(flimflam::kConnectedTechnologiesProperty,
                               ConnectedTechnologies(&error));
  adaptor_->EmitStringChanged(flimflam::kDefaultTechnologyProperty,
                              DefaultTechnology(&error));
}

string Manager::CalculateState(Error */*error*/) {
  return flimflam::kStateOffline;
}

vector<string> Manager::AvailableTechnologies(Error */*error*/) {
  set<string> unique_technologies;
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    unique_technologies.insert(
        Technology::NameFromIdentifier((*it)->technology()));
  }
  return vector<string>(unique_technologies.begin(), unique_technologies.end());
}

vector<string> Manager::ConnectedTechnologies(Error */*error*/) {
  set<string> unique_technologies;
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if ((*it)->IsConnected())
      unique_technologies.insert(
          Technology::NameFromIdentifier((*it)->technology()));
  }
  return vector<string>(unique_technologies.begin(), unique_technologies.end());
}

string Manager::DefaultTechnology(Error *error) {
  return (!services_.empty() && services_[0]->IsConnected()) ?
      services_[0]->GetTechnologyString(error) : "";
}

vector<string> Manager::EnabledTechnologies(Error *error) {
  // TODO(gauravsh): This must be wired up to the RPC interface to handle
  // enabled/disabled devices as set by the user. crosbug.com/23319
  return AvailableTechnologies(error);
}

vector<string> Manager::EnumerateDevices(Error */*error*/) {
  vector<string> device_rpc_ids;
  for (vector<DeviceRefPtr>::const_iterator it = devices_.begin();
       it != devices_.end();
       ++it) {
    device_rpc_ids.push_back((*it)->GetRpcIdentifier());
  }
  return device_rpc_ids;
}

vector<string> Manager::EnumerateAvailableServices(Error */*error*/) {
  vector<string> service_rpc_ids;
  for (vector<ServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    service_rpc_ids.push_back((*it)->GetRpcIdentifier());
  }
  return service_rpc_ids;
}

vector<string> Manager::EnumerateWatchedServices(Error *error) {
  // TODO(cmasone): Filter this list for services in appropriate states.
  return EnumerateAvailableServices(error);
}

string Manager::GetActiveProfileName(Error */*error*/) {
  return ActiveProfile()->GetFriendlyName();
}

// called via RPC (e.g., from ManagerDBusAdaptor)
WiFiServiceRefPtr Manager::GetWifiService(const KeyValueStore &args,
                                          Error *error) {
  vector<DeviceRefPtr> wifi_devices;
  FilterByTechnology(Technology::kWifi, &wifi_devices);
  if (wifi_devices.empty()) {
    error->Populate(Error::kInvalidArguments, kManagerErrorNoDevice);
    return NULL;
  } else {
    WiFi *wifi = dynamic_cast<WiFi *>(wifi_devices.front().get());
    CHECK(wifi);
    return wifi->GetService(args, error);
  }
}

// called via RPC (e.g., from ManagerDBusAdaptor)
void Manager::RequestScan(const string &technology, Error *error) {
  if (technology == flimflam::kTypeWifi || technology == "") {
    vector<DeviceRefPtr> wifi_devices;
    FilterByTechnology(Technology::kWifi, &wifi_devices);

    for (vector<DeviceRefPtr>::iterator it = wifi_devices.begin();
         it != wifi_devices.end();
         ++it) {
      (*it)->Scan(error);
    }
  } else {
    // TODO(quiche): support scanning for other technologies?
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Unrecognized technology " + technology);
  }
}

string Manager::GetTechnologyOrder() {
  vector<string> technology_names;
  for (vector<Technology::Identifier>::iterator it = technology_order_.begin();
       it != technology_order_.end();
       ++it) {
    technology_names.push_back(Technology::NameFromIdentifier(*it));
  }

  return JoinString(technology_names, ',');
}

void Manager::SetTechnologyOrder(const string &order, Error *error) {
  vector<Technology::Identifier> new_order;
  map<Technology::Identifier, bool> seen;

  vector<string> order_parts;
  base::SplitString(order, ',', &order_parts);

  for (vector<string>::iterator it = order_parts.begin();
       it != order_parts.end();
       ++it) {
    Technology::Identifier identifier = Technology::IdentifierFromName(*it);

    if (identifier == Technology::kUnknown) {
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            *it + " is an unknown technology name");
      return;
    }

    if (ContainsKey(seen, identifier)) {
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            *it + " is duplicated in the list");
      return;
    }
    seen[identifier] = true;
    new_order.push_back(identifier);
  }

  technology_order_ = new_order;
  SortServices();
}

}  // namespace shill
