// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <time.h>
#include <stdio.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/stringprintf.h>
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
#include "shill/hook_table.h"
#include "shill/key_file_store.h"
#include "shill/metrics.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/resolver.h"
#include "shill/scope_logger.h"
#include "shill/service.h"
#include "shill/service_sorter.h"
#include "shill/vpn_service.h"
#include "shill/wifi.h"
#include "shill/wifi_service.h"
#include "shill/wimax_service.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::set;
using std::string;
using std::vector;

namespace shill {

// statics
const char Manager::kErrorNoDevice[] = "no wifi devices available";
const char Manager::kErrorTypeRequired[] = "must specify service type";
const char Manager::kErrorUnsupportedServiceType[] =
    "service type is unsupported";

Manager::Manager(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 Metrics *metrics,
                 GLib *glib,
                 const string &run_directory,
                 const string &storage_directory,
                 const string &user_storage_format)
    : dispatcher_(dispatcher),
      run_path_(FilePath(run_directory)),
      storage_path_(FilePath(storage_directory)),
      user_storage_format_(user_storage_format),
      adaptor_(control_interface->CreateManagerAdaptor(this)),
      device_info_(control_interface, dispatcher, metrics, this),
      modem_info_(control_interface, dispatcher, metrics, this, glib),
      vpn_provider_(control_interface, dispatcher, metrics, this),
      wimax_provider_(control_interface, dispatcher, metrics, this),
      running_(false),
      connect_profiles_to_rpc_(true),
      ephemeral_profile_(new EphemeralProfile(control_interface, this)),
      control_interface_(control_interface),
      metrics_(metrics),
      glib_(glib),
      use_startup_portal_list_(false),
      termination_actions_(dispatcher) {
  HelpRegisterDerivedString(flimflam::kActiveProfileProperty,
                            &Manager::GetActiveProfileRpcIdentifier,
                            NULL);
  store_.RegisterBool(flimflam::kArpGatewayProperty, &props_.arp_gateway);
  HelpRegisterDerivedStrings(flimflam::kAvailableTechnologiesProperty,
                             &Manager::AvailableTechnologies,
                             NULL);
  HelpRegisterDerivedString(flimflam::kCheckPortalListProperty,
                            &Manager::GetCheckPortalList,
                            &Manager::SetCheckPortalList);
  HelpRegisterDerivedStrings(flimflam::kConnectedTechnologiesProperty,
                             &Manager::ConnectedTechnologies,
                             NULL);
  store_.RegisterString(flimflam::kCountryProperty, &props_.country);
  HelpRegisterDerivedString(flimflam::kDefaultTechnologyProperty,
                            &Manager::DefaultTechnology,
                            NULL);
  HelpRegisterConstDerivedRpcIdentifiers(flimflam::kDevicesProperty,
                                         &Manager::EnumerateDevices);
  HelpRegisterDerivedStrings(flimflam::kEnabledTechnologiesProperty,
                             &Manager::EnabledTechnologies,
                             NULL);
  store_.RegisterBool(flimflam::kOfflineModeProperty, &props_.offline_mode);
  store_.RegisterString(flimflam::kPortalURLProperty, &props_.portal_url);
  store_.RegisterInt32(kPortalCheckIntervalProperty,
                       &props_.portal_check_interval_seconds);
  HelpRegisterConstDerivedRpcIdentifiers(flimflam::kProfilesProperty,
                                         &Manager::EnumerateProfiles);
  store_.RegisterString(kHostNameProperty, &props_.host_name);
  HelpRegisterDerivedString(flimflam::kStateProperty,
                            &Manager::CalculateState,
                            NULL);
  HelpRegisterConstDerivedRpcIdentifiers(flimflam::kServicesProperty,
                                         &Manager::EnumerateAvailableServices);
  HelpRegisterConstDerivedRpcIdentifiers(flimflam::kServiceWatchListProperty,
                                         &Manager::EnumerateWatchedServices);

  // Set default technology order "by hand", to avoid invoking side
  // effects of SetTechnologyOrder.
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeVPN));
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeEthernet));
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeWimax));
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeWifi));
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeCellular));

  SLOG(Manager, 2) << "Manager initialized.";
}

Manager::~Manager() {
  profiles_.clear();
}

void Manager::AddDeviceToBlackList(const string &device_name) {
  device_info_.AddDeviceToBlackList(device_name);
}

void Manager::Start() {
  LOG(INFO) << "Manager started.";

  power_manager_.reset(new PowerManager(ProxyFactory::GetInstance()));
  // TODO(ers): weak ptr for metrics_?
  PowerManager::PowerStateCallback cb =
      Bind(&Metrics::NotifyPowerStateChange, Unretained(metrics_));
  power_manager_->AddStateChangeCallback(Metrics::kMetricPowerManagerKey, cb);

  CHECK(file_util::CreateDirectory(run_path_)) << run_path_.value();
  Resolver::GetInstance()->set_path(run_path_.Append("resolv.conf"));

  InitializeProfiles();
  running_ = true;
  adaptor_->UpdateRunning();
  device_info_.Start();
  modem_info_.Start();
  vpn_provider_.Start();
  wimax_provider_.Start();
}

void Manager::Stop() {
  running_ = false;
  // Persist profile, device, service information to disk.
  vector<ProfileRefPtr>::iterator it;
  for (it = profiles_.begin(); it != profiles_.end(); ++it) {
    // Since this happens in a loop, the current manager state is stored to
    // all default profiles in the stack.  This is acceptable because the
    // only time multiple default profiles are loaded are during autotests.
    (*it)->Save();
  }

  vector<ServiceRefPtr>::iterator services_it;
  Error e;
  for (services_it = services_.begin(); services_it != services_.end();
       ++services_it) {
    (*services_it)->Disconnect(&e);
  }

  adaptor_->UpdateRunning();
  wimax_provider_.Stop();
  vpn_provider_.Stop();
  modem_info_.Stop();
  device_info_.Stop();

  // Some unit tests do not call Manager::Start().
  if (power_manager_.get())
    power_manager_->RemoveStateChangeCallback(Metrics::kMetricPowerManagerKey);
}

void Manager::InitializeProfiles() {
  DCHECK(profiles_.empty());
  // The default profile must go first on the stack.
  CHECK(file_util::CreateDirectory(storage_path_)) << storage_path_.value();
  scoped_refptr<DefaultProfile>
      default_profile(new DefaultProfile(control_interface_,
                                         this,
                                         storage_path_,
                                         DefaultProfile::kDefaultId,
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
  SLOG(Manager, 2) << __func__ << " " << name;
  Profile::Identifier ident;
  if (!Profile::ParseIdentifier(name, &ident)) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Invalid profile name " + name);
    return;
  }

  ProfileRefPtr profile;
  if (ident.user.empty()) {
    profile = new DefaultProfile(control_interface_,
                                 this,
                                 storage_path_,
                                 ident.identifier,
                                 props_);
  } else {
    profile = new Profile(control_interface_,
                          this,
                          ident,
                          user_storage_format_,
                          true);
  }

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
  SLOG(Manager, 2) << __func__ << " " << name;
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

  ProfileRefPtr profile;
  if (ident.user.empty()) {
    // Allow a machine-wide-profile to be pushed on the stack only if the
    // profile stack is empty, or if the topmost profile on the stack is
    // also a machine-wide (non-user) profile.
    if (!profiles_.empty() && !profiles_.back()->GetUser().empty()) {
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            "Cannot load non-default global profile " + name +
                            " on top of a user profile");
      return;
    }

    scoped_refptr<DefaultProfile>
        default_profile(new DefaultProfile(control_interface_,
                                           this,
                                           storage_path_,
                                           ident.identifier,
                                           props_));
    if (!default_profile->InitStorage(glib_, Profile::kOpenExisting, error)) {
      // |error| will have been populated by InitStorage().
      return;
    }

    if (!default_profile->LoadManagerProperties(&props_)) {
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            "Could not load Manager properties from profile " +
                            name);
      return;
    }
    profile = default_profile;
  } else {
    profile = new Profile(control_interface_,
                          this,
                          ident,
                          user_storage_format_,
                          connect_profiles_to_rpc_);
    if (!profile->InitStorage(glib_, Profile::kOpenExisting, error)) {
      // |error| will have been populated by InitStorage().
      return;
    }
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

  // Offer the Profile contents to the service/device providers which will
  // create new services if necessary.
  vpn_provider_.CreateServicesFromProfile(profile);
  wimax_provider_.CreateServicesFromProfile(profile);

  *path = profile->GetRpcIdentifier();
  SortServices();
}

void Manager::PopProfileInternal() {
  CHECK(!profiles_.empty());
  ProfileRefPtr active_profile = profiles_.back();
  profiles_.pop_back();
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end();) {
    if ((*it)->profile().get() != active_profile.get() ||
        MatchProfileWithService(*it) ||
        !UnloadService(&it)) {
      LOG(ERROR) << "Skipping unload of service";
      ++it;
    }
  }
  SortServices();
}

void Manager::PopProfile(const string &name, Error *error) {
  SLOG(Manager, 2) << __func__ << " " << name;
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
  SLOG(Manager, 2) << __func__;
  Profile::Identifier ident;
  if (profiles_.empty()) {
    Error::PopulateAndLog(error, Error::kNotFound, "Profile stack is empty");
    return;
  }
  PopProfileInternal();
}

void Manager::RemoveProfile(const string &name, Error *error) {
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
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            "Cannot remove profile name " + name +
                            " since it is on stack");
      return;
    }
  }

  ProfileRefPtr profile;
  if (ident.user.empty()) {
    profile = new DefaultProfile(control_interface_,
                                 this,
                                 storage_path_,
                                 ident.identifier,
                                 props_);
  } else {
    profile = new Profile(control_interface_,
                          this,
                          ident,
                          user_storage_format_,
                          false);
  }


  // |error| will have been populated if RemoveStorage fails.
  profile->RemoveStorage(glib_, error);

  return;
}

bool Manager::HandleProfileEntryDeletion(const ProfileRefPtr &profile,
                                         const std::string &entry_name) {
  bool moved_services = false;
  for (vector<ServiceRefPtr>::iterator it = services_.begin();
       it != services_.end();) {
    if ((*it)->profile().get() == profile.get() &&
        (*it)->GetStorageIdentifier() == entry_name) {
      profile->AbandonService(*it);
      if (MatchProfileWithService(*it) ||
          !UnloadService(&it)) {
        ++it;
      }
      moved_services = true;
    } else {
      ++it;
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
      StringPrintf("Entry %s is not registered in the manager",
                   entry_name.c_str()));
  return NULL;
}

ServiceRefPtr Manager::GetServiceWithGUID(
    const std::string &guid, Error *error) {
  for (vector<ServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    if ((*it)->guid() == guid) {
      return *it;
    }
  }

  Error::PopulateAndLog(error, Error::kNotFound,
      StringPrintf("Service wth GUID %s is not registered in the manager",
                   guid.c_str()));
  return NULL;
}

ServiceRefPtr Manager::GetDefaultService() const {
  if (services_.empty() || !services_[0]->connection().get()) {
    SLOG(Manager, 2) << "In " << __func__ << ": No default connection exists.";
    return NULL;
  }
  return services_[0];
}

bool Manager::IsPortalDetectionEnabled(Technology::Identifier tech) {
  Error error;
  vector<Technology::Identifier> portal_technologies;
  return Technology::GetTechnologyVectorFromString(GetCheckPortalList(NULL),
                                                   &portal_technologies,
                                                   &error) &&
      std::find(portal_technologies.begin(), portal_technologies.end(),
                tech) != portal_technologies.end();
}

void Manager::SetStartupPortalList(const string &portal_list) {
  startup_portal_list_ = portal_list;
  use_startup_portal_list_ = true;
}

bool Manager::IsServiceEphemeral(const ServiceConstRefPtr &service) const {
  return service->profile() == ephemeral_profile_;
}

const ProfileRefPtr &Manager::ActiveProfile() const {
  DCHECK_NE(profiles_.size(), 0U);
  return profiles_.back();
}

bool Manager::IsActiveProfile(const ProfileRefPtr &profile) const {
  return (profiles_.size() > 0 &&
          ActiveProfile().get() == profile.get());
}

void Manager::SaveActiveProfile() {
  if (!profiles_.empty()) {
    ActiveProfile()->Save();
  }
}

bool Manager::MoveServiceToProfile(const ServiceRefPtr &to_move,
                                   const ProfileRefPtr &destination) {
  const ProfileRefPtr from = to_move->profile();
  SLOG(Manager, 2) << "Moving service "
                   << to_move->UniqueName()
                   << " to profile "
                   << destination->GetFriendlyName()
                   << " from "
                   << from->GetFriendlyName();
  return destination->AdoptService(to_move) &&
      from->AbandonService(to_move);
}

ProfileRefPtr Manager::LookupProfileByRpcIdentifier(
    const string &profile_rpcid) {
  for (vector<ProfileRefPtr>::iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    if (profile_rpcid == (*it)->GetRpcIdentifier()) {
      return *it;
    }
  }
  return NULL;
}

void Manager::SetProfileForService(const ServiceRefPtr &to_set,
                                   const string &profile_rpcid,
                                   Error *error) {
  ProfileRefPtr profile = LookupProfileByRpcIdentifier(profile_rpcid);
  if (!profile) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          StringPrintf("Unknown Profile %s requested for "
                                       "Service", profile_rpcid.c_str()));
    return;
  }

  if (to_set->profile().get() == profile.get()) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Service is already connected to this profile");
  } else if (!MoveServiceToProfile(to_set, profile)) {
    Error::PopulateAndLog(error, Error::kInternalError,
                          "Unable to move service to profile");
  }
}

void Manager::EnableTechnology(const std::string &technology_name,
                               Error *error,
                               const ResultCallback &callback) {
  CHECK(error != NULL);
  DCHECK(error->IsOngoing());
  Technology::Identifier id = Technology::IdentifierFromName(technology_name);
  if (id == Technology::kUnknown) {
    error->Populate(Error::kInvalidArguments, "Unknown technology");
    return;
  }
  bool deferred = false;
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    DeviceRefPtr device = *it;
    if (device->technology() == id && !device->enabled()) {
      device->SetEnabledPersistent(true, error, callback);
      // Continue with other devices even if one fails
      // TODO(ers): Decide whether an error should be returned
      // for the overall EnableTechnology operation if some
      // devices succeed and some fail.
      if (error->IsOngoing())
        deferred = true;
    }
  }
  // If no device has deferred work, then clear the error to
  // communicate to the caller that all work is done.
  if (!deferred)
    error->Reset();
}

void Manager::DisableTechnology(const std::string &technology_name,
                                Error *error,
                                const ResultCallback &callback) {
  CHECK(error != NULL);
  DCHECK(error->IsOngoing());
  Technology::Identifier id = Technology::IdentifierFromName(technology_name);
  if (id == Technology::kUnknown) {
    error->Populate(Error::kInvalidArguments, "Unknown technology");
    return;
  }
  bool deferred = false;
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    DeviceRefPtr device = *it;
    if (device->technology() == id && device->enabled()) {
      device->SetEnabledPersistent(false, error, callback);
      // Continue with other devices even if one fails
      // TODO(ers): Decide whether an error should be returned
      // for the overall DisableTechnology operation if some
      // devices succeed and some fail.
      if (error->IsOngoing())
        deferred = true;
    }
  }
  // If no device has deferred work, then clear the error to
  // communicate to the caller that all work is done.
  if (!deferred)
    error->Reset();
}

void Manager::UpdateEnabledTechnologies() {
  Error error;
  adaptor_->EmitStringsChanged(flimflam::kEnabledTechnologiesProperty,
                               EnabledTechnologies(&error));
}

void Manager::RegisterDevice(const DeviceRefPtr &to_manage) {
  SLOG(Manager, 2) << __func__ << "(" << to_manage->FriendlyName() << ")";
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
  if (running_ && (to_manage->enabled_persistent() ||
                   to_manage->IsUnderlyingDeviceEnabled()))
    to_manage->SetEnabled(true);

  EmitDeviceProperties();
}

void Manager::DeregisterDevice(const DeviceRefPtr &to_forget) {
  SLOG(Manager, 2) << __func__ << "(" << to_forget->FriendlyName() << ")";
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_forget.get() == it->get()) {
      SLOG(Manager, 2) << "Deregistered device: " << to_forget->UniqueName();
      to_forget->SetEnabled(false);
      devices_.erase(it);
      EmitDeviceProperties();
      return;
    }
  }
  SLOG(Manager, 2) << __func__ << " unknown device: "
                   << to_forget->UniqueName();
}

void Manager::EmitDeviceProperties() {
  vector<DeviceRefPtr>::iterator it;
  vector<string> device_paths;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    device_paths.push_back((*it)->GetRpcIdentifier());
  }
  adaptor_->EmitRpcIdentifierArrayChanged(flimflam::kDevicesProperty,
                                          device_paths);
  Error error;
  adaptor_->EmitStringsChanged(flimflam::kAvailableTechnologiesProperty,
                               AvailableTechnologies(&error));
  adaptor_->EmitStringsChanged(flimflam::kEnabledTechnologiesProperty,
                               EnabledTechnologies(&error));
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
  SLOG(Manager, 2) << "In " << __func__ << "(): Registering service "
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

bool Manager::UnloadService(vector<ServiceRefPtr>::iterator *service_iterator) {
  if (!(**service_iterator)->Unload()) {
    return false;
  }

  DCHECK(!(**service_iterator)->connection());
  *service_iterator = services_.erase(*service_iterator);

  return true;
}

void Manager::UpdateService(const ServiceRefPtr &to_update) {
  CHECK(to_update);
  LOG(INFO) << "Service " << to_update->UniqueName() << " updated;"
            << " state: " << Service::ConnectStateToString(to_update->state())
            << " failure: "
            << Service::ConnectFailureToString(to_update->failure());
  SLOG(Manager, 2) << "IsConnected(): " << to_update->IsConnected();
  SLOG(Manager, 2) << "IsConnecting(): " << to_update->IsConnecting();
  if (to_update->IsConnected()) {
    to_update->MakeFavorite();
    // Persists the updated favorite setting in the profile.
    SaveServiceToProfile(to_update);
  }
  SortServices();
}

void Manager::SaveServiceToProfile(const ServiceRefPtr &to_update) {
  if (IsServiceEphemeral(to_update)) {
    if (profiles_.empty()) {
      LOG(ERROR) << "Cannot assign profile to service: no profiles exist!";
    } else {
      MoveServiceToProfile(to_update, profiles_.back());
    }
  } else {
    to_update->profile()->UpdateService(to_update);
  }
}

void Manager::AddTerminationAction(const std::string &name,
                                   const base::Closure &start) {
  termination_actions_.Add(name, start);
}

void Manager::TerminationActionComplete(const std::string &name) {
  termination_actions_.ActionComplete(name);
}

void Manager::RemoveTerminationAction(const std::string &name) {
  termination_actions_.Remove(name);
}

void Manager::RunTerminationActions(
    int timeout_ms, const base::Callback<void(const Error &)> &done) {
  termination_actions_.Run(timeout_ms, done);
}

void Manager::FilterByTechnology(Technology::Identifier tech,
                                 vector<DeviceRefPtr> *found) {
  CHECK(found);
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if ((*it)->technology() == tech)
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

void Manager::HelpRegisterConstDerivedRpcIdentifiers(
    const string &name,
    RpcIdentifiers(Manager::*get)(Error *error)) {
  store_.RegisterDerivedRpcIdentifiers(
      name,
      RpcIdentifiersAccessor(
          new CustomAccessor<Manager, RpcIdentifiers>(this, get, NULL)));
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
  SLOG(Manager, 4) << "In " << __func__;
  ServiceRefPtr default_service;

  if (!services_.empty()) {
    // Keep track of the service that is the candidate for the default
    // service.  We have not yet tested to see if this service has a
    // connection.
    default_service = services_[0];
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
    ConnectionRefPtr default_connection = default_service->connection();
    if (default_connection.get() &&
        (services_[0]->connection().get() != default_connection.get())) {
      default_connection->SetIsDefault(false);
    }
    if (services_[0]->connection().get()) {
      services_[0]->connection()->SetIsDefault(true);
      default_service = services_[0];
    } else {
      default_service = NULL;
    }
  }
  metrics_->NotifyDefaultServiceChanged(default_service);

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
  dispatcher_->PostTask(Bind(&Manager::AutoConnectTask, AsWeakPtr()));
}

void Manager::AutoConnectTask() {
  if (services_.empty()) {
    LOG(INFO) << "No services.";
    return;
  }

  if (SLOG_IS_ON(Manager, 4)) {
    SLOG(Manager, 4) << "Sorted service list: ";
    for (size_t i = 0; i < services_.size(); ++i) {
      ServiceRefPtr service = services_[i];
      const char *compare_reason = NULL;
      if (i + 1 < services_.size()) {
        Service::Compare(
            service, services_[i+1], technology_order_, &compare_reason);
      } else {
        compare_reason = "last";
      }
      SLOG(Manager, 4) << "Service " << service->friendly_name()
                       << " IsConnected: " << service->IsConnected()
                       << " IsConnecting: " << service->IsConnecting()
                       << " IsFailed: " << service->IsFailed()
                       << " connectable: " << service->connectable()
                       << " auto_connect: " << service->auto_connect()
                       << " favorite: " << service->favorite()
                       << " priority: " << service->priority()
                       << " security_level: " << service->security_level()
                       << " strength: " << service->strength()
                       << " UniqueName: " << service->UniqueName()
                       << " sorted: " << compare_reason;
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
  // |services_| is sorted such that connected services are first.
  if (!services_.empty() &&
      services_.front()->IsConnected()) {
    return flimflam::kStateOnline;
  }
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

string Manager::DefaultTechnology(Error */*error*/) {
  return (!services_.empty() && services_[0]->IsConnected()) ?
      services_[0]->GetTechnologyString() : "";
}

vector<string> Manager::EnabledTechnologies(Error */*error*/) {
  set<string> unique_technologies;
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if ((*it)->enabled())
      unique_technologies.insert(
          Technology::NameFromIdentifier((*it)->technology()));
  }
  return vector<string>(unique_technologies.begin(), unique_technologies.end());
}

RpcIdentifiers Manager::EnumerateDevices(Error */*error*/) {
  RpcIdentifiers device_rpc_ids;
  for (vector<DeviceRefPtr>::const_iterator it = devices_.begin();
       it != devices_.end();
       ++it) {
    device_rpc_ids.push_back((*it)->GetRpcIdentifier());
  }
  return device_rpc_ids;
}

RpcIdentifiers Manager::EnumerateProfiles(Error */*error*/) {
  RpcIdentifiers profile_rpc_ids;
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

RpcIdentifiers Manager::EnumerateWatchedServices(Error *error) {
  // TODO(cmasone): Filter this list for services in appropriate states.
  return EnumerateAvailableServices(error);
}

string Manager::GetActiveProfileRpcIdentifier(Error */*error*/) {
  return ActiveProfile()->GetRpcIdentifier();
}

string Manager::GetCheckPortalList(Error */*error*/) {
  return use_startup_portal_list_ ? startup_portal_list_ :
      props_.check_portal_list;
}

void Manager::SetCheckPortalList(const string &portal_list, Error *error) {
  props_.check_portal_list = portal_list;
  use_startup_portal_list_ = false;
}

// called via RPC (e.g., from ManagerDBusAdaptor)
ServiceRefPtr Manager::GetService(const KeyValueStore &args, Error *error) {
  if (args.ContainsString(flimflam::kTypeProperty) &&
      args.GetString(flimflam::kTypeProperty) == flimflam::kTypeVPN) {
     // GetService on a VPN service should actually perform ConfigureService.
     // TODO(pstew): Remove this hack and change Chrome to use ConfigureService
     // instead, when we no longer need to support flimflam.  crosbug.com/31523
     return ConfigureService(args, error);
  }
  return GetServiceInner(args, error);
}

ServiceRefPtr Manager::GetServiceInner(const KeyValueStore &args,
                                       Error *error) {
  if (args.ContainsString(flimflam::kGuidProperty)) {
    SLOG(Manager, 2) << __func__ << ": searching by GUID";
    ServiceRefPtr service =
        GetServiceWithGUID(args.GetString(flimflam::kGuidProperty), NULL);
    if (service) {
      service->Configure(args, error);
      return service;
    }
  }

  if (!args.ContainsString(flimflam::kTypeProperty)) {
    Error::PopulateAndLog(error, Error::kInvalidArguments, kErrorTypeRequired);
    return NULL;
  }

  string type = args.GetString(flimflam::kTypeProperty);
  if (type == flimflam::kTypeVPN) {
    SLOG(Manager, 2) << __func__ << ": getting VPN Service";
    return vpn_provider_.GetService(args, error);
  }
  if (type == flimflam::kTypeWifi) {
    SLOG(Manager, 2) << __func__ << ": getting WiFi Service";
    return GetWifiService(args, error);
  }
  if (type == flimflam::kTypeWimax) {
    SLOG(Manager, 2) << __func__ << ": getting WiMAX Service";
    return wimax_provider_.GetService(args, error);
  }
  Error::PopulateAndLog(error, Error::kNotSupported,
                        kErrorUnsupportedServiceType);
  return NULL;
}

WiFiServiceRefPtr Manager::GetWifiService(const KeyValueStore &args,
                                          Error *error) {
  vector<DeviceRefPtr> wifi_devices;
  FilterByTechnology(Technology::kWifi, &wifi_devices);
  if (wifi_devices.empty()) {
    Error::PopulateAndLog(error, Error::kInvalidArguments, kErrorNoDevice);
    return NULL;
  } else {
    WiFi *wifi = dynamic_cast<WiFi *>(wifi_devices.front().get());
    CHECK(wifi);
    return wifi->GetService(args, error);
  }
}

// called via RPC (e.g., from ManagerDBusAdaptor)
ServiceRefPtr Manager::ConfigureService(const KeyValueStore &args,
                                        Error *error) {
  ProfileRefPtr profile = ActiveProfile();
  bool profile_specified = args.ContainsString(flimflam::kProfileProperty);
  if (profile_specified) {
    string profile_rpcid = args.GetString(flimflam::kProfileProperty);
    profile = LookupProfileByRpcIdentifier(profile_rpcid);
    if (!profile) {
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            "Invalid profile name " + profile_rpcid);
      return NULL;
    }
  }

  ServiceRefPtr service = GetServiceInner(args, error);
  if (error->IsFailure() || !service) {
    LOG(ERROR) << "GetService failed; returning upstream error.";
    return NULL;
  }

  // Overwrite the profile data with the resulting configured service.
  if (!profile->UpdateService(service)) {
    Error::PopulateAndLog(error, Error::kInternalError,
                          "Unable to save service to profile");
    return NULL;
  }

  if (HasService(service)) {
    // If the service has been registered (it may not be -- as is the case
    // with invisible WiFi networks), we can now transfer the service between
    // profiles.
    if (IsServiceEphemeral(service) ||
        (profile_specified && service->profile() != profile)) {
      SLOG(Manager, 2) << "Moving service to profile "
                       << profile->GetFriendlyName();
      if (!MoveServiceToProfile(service, profile)) {
        Error::PopulateAndLog(error, Error::kInternalError,
                              "Unable to move service to profile");
      }
    }
  }

  // Notify the service that a profile has been configured for it.
  service->OnProfileConfigured();

  return service;
}

void Manager::RecheckPortal(Error */*error*/) {
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if ((*it)->RequestPortalDetection()) {
      // Only start Portal Detection on the device with the default connection.
      // We will get a "true" return value when we've found that device, and
      // can end our loop early as a result.
      break;
    }
  }
}

void Manager::RecheckPortalOnService(const ServiceRefPtr &service) {
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if ((*it)->IsConnectedToService(service)) {
      // As opposed to RecheckPortal() above, we explicitly stop and then
      // restart portal detection, since the service to recheck was explicitly
      // specified.
      (*it)->RestartPortalDetection();
      break;
    }
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
  SLOG(Manager, 2) << "Setting technology order to " << order;
  if (!Technology::GetTechnologyVectorFromString(order, &new_order, error)) {
    return;
  }

  technology_order_ = new_order;
  SortServices();
}

}  // namespace shill
