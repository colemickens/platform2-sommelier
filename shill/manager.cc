// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
#include "shill/connection.h"
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
                 Metrics *metrics,
                 GLib *glib,
                 const string &run_directory,
                 const string &storage_directory,
                 const string &user_storage_format)
    : dispatcher_(dispatcher),
      task_factory_(this),
      run_path_(FilePath(run_directory)),
      storage_path_(FilePath(storage_directory)),
      user_storage_format_(user_storage_format),
      adaptor_(control_interface->CreateManagerAdaptor(this)),
      device_info_(control_interface, dispatcher, metrics, this),
      modem_info_(control_interface, dispatcher, metrics, this, glib),
      running_(false),
      connect_profiles_to_rpc_(true),
      ephemeral_profile_(new EphemeralProfile(control_interface, this)),
      control_interface_(control_interface),
      metrics_(metrics),
      glib_(glib) {
  HelpRegisterDerivedString(flimflam::kActiveProfileProperty,
                            &Manager::GetActiveProfileRpcIdentifier,
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
  HelpRegisterDerivedStrings(flimflam::kProfilesProperty,
                             &Manager::EnumerateProfiles,
                             NULL);
  store_.RegisterString(shill::kHostNameProperty, &props_.host_name);
  HelpRegisterDerivedString(flimflam::kStateProperty,
                            &Manager::CalculateState,
                            NULL);
  HelpRegisterDerivedStrings(flimflam::kServicesProperty,
                             &Manager::EnumerateAvailableServices,
                             NULL);
  HelpRegisterDerivedStrings(flimflam::kServiceWatchListProperty,
                             &Manager::EnumerateWatchedServices,
                             NULL);

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

  InitializeProfiles();
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
  Error e;
  for (services_it = services_.begin(); services_it != services_.end();
       ++services_it) {
    (*services_it)->Disconnect(&e);
  }

  adaptor_->UpdateRunning();
  modem_info_.Stop();
  device_info_.Stop();
}

void Manager::InitializeProfiles() {
  DCHECK(profiles_.empty());
  // The default profile must go first on the stack.
  CHECK(file_util::CreateDirectory(storage_path_)) << storage_path_.value();
  scoped_refptr<DefaultProfile>
      default_profile(new DefaultProfile(control_interface_,
                                         this,
                                         storage_path_,
                                         props_));
  CHECK(default_profile->InitStorage(glib_, Profile::kCreateOrOpenExisting,
                                     NULL));
  CHECK(default_profile->LoadManagerProperties(&props_));
  profiles_.push_back(default_profile.release());
  Error error;
  string path;
  for (vector<string>::iterator it = startup_profiles_.begin();
       it != startup_profiles_.end(); ++it)
    PushProfile(*it, &path, &error);
}

void Manager::CreateProfile(const string &name, string *path, Error *error) {
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
                                    connect_profiles_to_rpc_));
  if (!profile->InitStorage(glib_, Profile::kCreateNew, error)) {
    // |error| will have been populated by InitStorage().
    return;
  }

  // Save profile data out, and then let the scoped pointer fall out of scope.
  if (!profile->Save()) {
    Error::PopulateAndLog(error, Error::kInternalError,
                          "Profile name " + name + " could not be saved");
    return;
  }

  *path = profile->GetRpcIdentifier();
}

void Manager::PushProfile(const string &name, string *path, Error *error) {
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
    // crosbug.com/24461
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
    // |error| will have been populated by InitStorage().
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

  *path = profile->GetRpcIdentifier();
  SortServices();
}

void Manager::PopProfileInternal() {
  CHECK(!profiles_.empty());
  ProfileRefPtr active_profile = profiles_.back();
  profiles_.pop_back();
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if ((*it)->profile().get() == active_profile.get() &&
        !MatchProfileWithService(*it)) {
      (*it)->Unload();
    }
  }
  SortServices();
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

bool Manager::HandleProfileEntryDeletion(const ProfileRefPtr &profile,
                                         const std::string &entry_name) {
  bool moved_services = false;
  for (vector<ServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    if ((*it)->profile().get() == profile.get() &&
        (*it)->GetStorageIdentifier() == entry_name) {
      profile->AbandonService(*it);
      if (!MatchProfileWithService(*it)) {
        (*it)->Unload();
      }
      moved_services = true;
    }
  }
  return moved_services;
}

ServiceRefPtr Manager::GetServiceWithStorageIdentifier(
    const ProfileRefPtr &profile, const std::string &entry_name, Error *error) {
  for (vector<ServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    if ((*it)->profile().get() == profile.get() &&
        (*it)->GetStorageIdentifier() == entry_name) {
      return *it;
    }
  }

  Error::PopulateAndLog(error, Error::kNotFound,
      base::StringPrintf("Entry %s is not registered in the manager",
                         entry_name.c_str()));
  return NULL;
}

const ProfileRefPtr &Manager::ActiveProfile() const {
  DCHECK_NE(profiles_.size(), 0);
  return profiles_.back();
}

bool Manager::IsActiveProfile(const ProfileRefPtr &profile) const {
  return (profiles_.size() > 0 &&
          ActiveProfile().get() == profile.get());
}

bool Manager::MoveServiceToProfile(const ServiceRefPtr &to_move,
                                   const ProfileRefPtr &destination) {
  const ProfileRefPtr from = to_move->profile();
  VLOG(2) << "Moving service "
          << to_move->UniqueName()
          << " to profile "
          << destination->GetFriendlyName()
          << " from "
          << from->GetFriendlyName();
  return destination->AdoptService(to_move) &&
      from->AbandonService(to_move);
}

void Manager::SetProfileForService(const ServiceRefPtr &to_set,
                                   const string &profile_rpcid,
                                   Error *error) {
  for (vector<ProfileRefPtr>::iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    if (profile_rpcid == (*it)->GetRpcIdentifier()) {
      if (to_set->profile().get() == it->get()) {
        Error::PopulateAndLog(error, Error::kInvalidArguments,
                              "Service is already connected to this profile");
      } else if (!MoveServiceToProfile(to_set, *it)) {
        Error::PopulateAndLog(error, Error::kInternalError,
                              "Unable to move service to profile");
      }
      return;
    }
  }
  Error::PopulateAndLog(error, Error::kInvalidArguments,
                        "Unknown Profile requested for Service");
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

bool Manager::HasService(const ServiceRefPtr &service) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if ((*it)->UniqueName() == service->UniqueName())
      return true;
  }
  return false;
}

void Manager::RegisterService(const ServiceRefPtr &to_manage) {
  VLOG(2) << "In " << __func__ << "(): Registering service "
          << to_manage->UniqueName();

  MatchProfileWithService(to_manage);

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
      DCHECK(!(*it)->connection());
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
  VLOG(2) << "IsConnected(): " << to_update->IsConnected();
  VLOG(2) << "IsConnecting(): " << to_update->IsConnecting();
  if (to_update->IsConnected()) {
    to_update->MakeFavorite();
    if (to_update->profile().get() == ephemeral_profile_.get()) {
      if (profiles_.empty()) {
        LOG(ERROR) << "Cannot assign profile to service: no profiles exist!";
      } else {
        MoveServiceToProfile(to_update, profiles_.back());
      }
    }
  }
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
  VLOG(4) << "In " << __func__;
  ConnectionRefPtr default_connection;
  if (!services_.empty()) {
    // Keep track of the connection that was last considered default.
    default_connection = services_[0]->connection();
  }
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

  if (!services_.empty()) {
    if (default_connection.get() &&
        (services_[0]->connection().get() != default_connection.get())) {
      default_connection->SetIsDefault(false);
    }
    if (services_[0]->connection().get()) {
      services_[0]->connection()->SetIsDefault(true);
    }
  }

  AutoConnect();
}

bool Manager::MatchProfileWithService(const ServiceRefPtr &service) {
  vector<ProfileRefPtr>::reverse_iterator it;
  for (it = profiles_.rbegin(); it != profiles_.rend(); ++it) {
    if ((*it)->ConfigureService(service)) {
      break;
    }
  }
  if (it == profiles_.rend()) {
    ephemeral_profile_->AdoptService(service);
    return false;
  }
  return true;
}

void Manager::AutoConnect() {
  // We might be called in the middle of another request (e.g., as a
  // consequence of Service::SetState calling UpdateService). To avoid
  // re-entrancy issues in dbus-c++, defer to the event loop.
  dispatcher_->PostTask(
      task_factory_.NewRunnableMethod(&Manager::AutoConnectTask));
  return;
}

void Manager::AutoConnectTask() {
  if (services_.empty()) {
    LOG(INFO) << "No services.";
    return;
  }

  if (VLOG_IS_ON(4)) {
    VLOG(4) << "Sorted service list: ";  // XXX check log
    for (vector<ServiceRefPtr>::const_iterator it = services_.begin();
         it != services_.end(); ++it) {
      VLOG(4) << "Service " << (*it)->friendly_name()
              << " IsConnected: " << (*it)->IsConnected()
              << " IsConnecting: " << (*it)->IsConnecting()
              << " IsFailed: " << (*it)->IsFailed()
              << " connectable: " << (*it)->connectable()
              << " auto_connect: " << (*it)->auto_connect()
              << " favorite: " << (*it)->favorite()
              << " priority: " << (*it)->priority()
              << " security_level: " << (*it)->security_level()
              << " strength: " << (*it)->strength()
              << " UniqueName: " << (*it)->UniqueName();
    }
  }

  // Perform auto-connect.
  for (vector<ServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    if ((*it)->auto_connect()) {
      LOG(INFO) << "Requesting autoconnect to service "
                << (*it)->friendly_name() << ".";
      (*it)->AutoConnect();
    }
  }
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

vector<string> Manager::EnumerateProfiles(Error */*error*/) {
  vector<string> profile_rpc_ids;
  for (vector<ProfileRefPtr>::const_iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    profile_rpc_ids.push_back((*it)->GetRpcIdentifier());
  }
  return profile_rpc_ids;
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

string Manager::GetActiveProfileRpcIdentifier(Error */*error*/) {
  return ActiveProfile()->GetRpcIdentifier();
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
