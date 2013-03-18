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
#include <base/memory/ref_counted.h>
#include <base/stringprintf.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/connection.h"
#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_manager.h"
#include "shill/default_profile.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/ephemeral_profile.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/geolocation_info.h"
#include "shill/hook_table.h"
#include "shill/key_file_store.h"
#include "shill/logging.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/resolver.h"
#include "shill/service.h"
#include "shill/service_sorter.h"
#include "shill/vpn_provider.h"
#include "shill/vpn_service.h"
#include "shill/wifi.h"
#include "shill/wifi_provider.h"
#include "shill/wifi_service.h"
#include "shill/wimax_service.h"

using base::Bind;
using base::FilePath;
using base::StringPrintf;
using base::Unretained;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

namespace {

// Human-readable string describing the suspend delay that is registered
// with the power manager.
const char kSuspendDelayDescription[] = "shill";

}  // namespace

// statics
const char Manager::kErrorNoDevice[] = "no wifi devices available";
const char Manager::kErrorTypeRequired[] = "must specify service type";
const char Manager::kErrorUnsupportedServiceType[] =
    "service type is unsupported";
// This timeout should be less than the upstart job timeout, otherwise
// stats for termination actions might be lost.
const int Manager::kTerminationActionsTimeoutMilliseconds = 4500;
const char Manager::kPowerManagerKey[] = "manager";

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
      vpn_provider_(
          new VPNProvider(control_interface, dispatcher, metrics, this)),
      wifi_provider_(
          new WiFiProvider(control_interface, dispatcher, metrics, this)),
      wimax_provider_(control_interface, dispatcher, metrics, this),
      resolver_(Resolver::GetInstance()),
      running_(false),
      connect_profiles_to_rpc_(true),
      ephemeral_profile_(
          new EphemeralProfile(control_interface, metrics, this)),
      control_interface_(control_interface),
      metrics_(metrics),
      glib_(glib),
      use_startup_portal_list_(false),
      termination_actions_(dispatcher),
      suspend_delay_registered_(false),
      suspend_delay_id_(0),
      default_service_callback_tag_(0) {
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
  HelpRegisterConstDerivedRpcIdentifier(
      shill::kDefaultServiceProperty,
      &Manager::GetDefaultServiceRpcIdentifier);
  HelpRegisterConstDerivedRpcIdentifiers(flimflam::kDevicesProperty,
                                         &Manager::EnumerateDevices);
  HelpRegisterDerivedStrings(flimflam::kEnabledTechnologiesProperty,
                             &Manager::EnabledTechnologies,
                             NULL);
  HelpRegisterDerivedString(shill::kIgnoredDNSSearchPathsProperty,
                            &Manager::GetIgnoredDNSSearchPaths,
                            &Manager::SetIgnoredDNSSearchPaths);
  store_.RegisterString(shill::kLinkMonitorTechnologiesProperty,
                        &props_.link_monitor_technologies);
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
  HelpRegisterConstDerivedRpcIdentifiers(shill::kServiceCompleteListProperty,
                                         &Manager::EnumerateCompleteServices);
  HelpRegisterConstDerivedRpcIdentifiers(flimflam::kServiceWatchListProperty,
                                         &Manager::EnumerateWatchedServices);
  store_.RegisterString(shill::kShortDNSTimeoutTechnologiesProperty,
                        &props_.short_dns_timeout_technologies);
  HelpRegisterDerivedStrings(kUninitializedTechnologiesProperty,
                             &Manager::UninitializedTechnologies,
                             NULL);

  // Set default technology order "by hand", to avoid invoking side
  // effects of SetTechnologyOrder.
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeVPN));
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeEthernet));
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeWifi));
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeWimax));
  technology_order_.push_back(
      Technology::IdentifierFromName(flimflam::kTypeCellular));

  SLOG(Manager, 2) << "Manager initialized.";
}

Manager::~Manager() {}

void Manager::AddDeviceToBlackList(const string &device_name) {
  device_info_.AddDeviceToBlackList(device_name);
}

void Manager::Start() {
  LOG(INFO) << "Manager started.";

  dbus_manager_.reset(new DBusManager());
  dbus_manager_->Start();

  power_manager_.reset(
      new PowerManager(dispatcher_, ProxyFactory::GetInstance()));
  power_manager_->AddStateChangeCallback(
      kPowerManagerKey,
      Bind(&Manager::OnPowerStateChanged, AsWeakPtr()));
  // TODO(ers): weak ptr for metrics_?
  PowerManager::PowerStateCallback cb =
      Bind(&Metrics::NotifyPowerStateChange, Unretained(metrics_));
  power_manager_->AddStateChangeCallback(Metrics::kMetricPowerManagerKey, cb);

  CHECK(file_util::CreateDirectory(run_path_)) << run_path_.value();
  resolver_->set_path(run_path_.Append("resolv.conf"));

  InitializeProfiles();
  running_ = true;
  adaptor_->UpdateRunning();
  device_info_.Start();
  modem_info_.Start();
  vpn_provider_->Start();
  wifi_provider_->Start();
  wimax_provider_.Start();
}

void Manager::Stop() {
  running_ = false;
  // Persist device information to disk;
  vector<DeviceRefPtr>::iterator devices_it;
  for (devices_it = devices_.begin(); devices_it != devices_.end();
       ++devices_it) {
    UpdateDevice(*devices_it);
  }

  // Persist profile, service information to disk.
  vector<ProfileRefPtr>::iterator profiles_it;
  for (profiles_it = profiles_.begin(); profiles_it != profiles_.end();
       ++profiles_it) {
    // Since this happens in a loop, the current manager state is stored to
    // all default profiles in the stack.  This is acceptable because the
    // only time multiple default profiles are loaded are during autotests.
    (*profiles_it)->Save();
  }

  vector<ServiceRefPtr>::iterator services_it;
  Error e;
  for (services_it = services_.begin(); services_it != services_.end();
       ++services_it) {
    (*services_it)->Disconnect(&e);
  }

  adaptor_->UpdateRunning();
  wimax_provider_.Stop();
  wifi_provider_->Stop();
  vpn_provider_->Stop();
  modem_info_.Stop();
  device_info_.Stop();
  sort_services_task_.Cancel();
  power_manager_.reset();
  dbus_manager_.reset();
}

void Manager::InitializeProfiles() {
  DCHECK(profiles_.empty());
  // The default profile must go first on the stack.
  CHECK(file_util::CreateDirectory(storage_path_)) << storage_path_.value();
  scoped_refptr<DefaultProfile>
      default_profile(new DefaultProfile(control_interface_,
                                         metrics_,
                                         this,
                                         storage_path_,
                                         DefaultProfile::kDefaultId,
                                         props_));
  // The default profile may fail to initialize if it's corrupted.
  // If so, recreate the default profile.
  if (!default_profile->InitStorage(
      glib_, Profile::kCreateOrOpenExisting, NULL))
    CHECK(default_profile->InitStorage(glib_, Profile::kCreateNew,
                                       NULL));
  CHECK(LoadProperties(default_profile));
  profiles_.push_back(default_profile);
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

  for (vector<ProfileRefPtr>::const_iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    if ((*it)->MatchesIdentifier(ident)) {
      Error::PopulateAndLog(error, Error::kAlreadyExists,
                            "Profile name " + name + " is already on stack");
      *path = (*it)->GetRpcIdentifier();
      return;
    }
  }

  ProfileRefPtr profile;
  if (ident.user.empty()) {
    profile = new DefaultProfile(control_interface_,
                                 metrics_,
                                 this,
                                 storage_path_,
                                 ident.identifier,
                                 props_);
  } else {
    profile = new Profile(control_interface_,
                          metrics_,
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
                                           metrics_,
                                           this,
                                           storage_path_,
                                           ident.identifier,
                                           props_));
    if (!default_profile->InitStorage(glib_, Profile::kOpenExisting, error)) {
      // |error| will have been populated by InitStorage().
      return;
    }

    if (!LoadProperties(default_profile)) {
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            "Could not load Manager properties from profile " +
                            name);
      return;
    }
    profile = default_profile;
  } else {
    profile = new Profile(control_interface_,
                          metrics_,
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

  // Shop the Profile contents around to Devices which may have configuration
  // stored in these profiles.
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    profile->ConfigureDevice(*it);
  }

  // Offer the Profile contents to the service/device providers which will
  // create new services if necessary.
  vpn_provider_->CreateServicesFromProfile(profile);
  wifi_provider_->CreateServicesFromProfile(profile);
  wimax_provider_.CreateServicesFromProfile(profile);

  *path = profile->GetRpcIdentifier();
  SortServices();

  Error unused_error;
  adaptor_->EmitStringsChanged(flimflam::kProfilesProperty,
                               EnumerateProfiles(&unused_error));
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

  Error unused_error;
  adaptor_->EmitStringsChanged(flimflam::kProfilesProperty,
                               EnumerateProfiles(&unused_error));
}

void Manager::PopProfile(const string &name, Error *error) {
  SLOG(Manager, 2) << __func__ << " " << name;
  // This signal is sent when a user logs out of a session.  Regardless of
  // whether we find their profile to remove, lets clear the network related
  // logs.
  MemoryLog::GetInstance()->Clear();
  LOG(INFO) << "Cleared the memory log on logout event.";
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
                                 metrics_,
                                 this,
                                 storage_path_,
                                 ident.identifier,
                                 props_);
  } else {
    profile = new Profile(control_interface_,
                          metrics_,
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

  string error_string(
      StringPrintf("Entry %s is not registered in the manager",
                   entry_name.c_str()));
  if (error) {
    error->Populate(Error::kNotFound, error_string);
  }
  SLOG(Manager, 2) << error_string;
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

  string error_string(
      StringPrintf("Service wth GUID %s is not registered in the manager",
                   guid.c_str()));
  if (error) {
    error->Populate(Error::kNotFound, error_string);
  }
  SLOG(Manager, 2) << error_string;
  return NULL;
}

ServiceRefPtr Manager::GetDefaultService() const {
  SLOG(Manager, 2) << __func__;
  if (services_.empty() || !services_[0]->connection().get()) {
    SLOG(Manager, 2) << "In " << __func__ << ": No default connection exists.";
    return NULL;
  }
  return services_[0];
}

RpcIdentifier Manager::GetDefaultServiceRpcIdentifier(Error */*error*/) {
  ServiceRefPtr default_service = GetDefaultService();
  return default_service ? default_service->GetRpcIdentifier() : "/";
}

bool Manager::IsTechnologyInList(const string &technology_list,
                                 Technology::Identifier tech) const {
  Error error;
  vector<Technology::Identifier> technologies;
  return Technology::GetTechnologyVectorFromString(technology_list,
                                                   &technologies,
                                                   &error) &&
      std::find(technologies.begin(), technologies.end(), tech) !=
          technologies.end();
}

bool Manager::IsPortalDetectionEnabled(Technology::Identifier tech) {
  return IsTechnologyInList(GetCheckPortalList(NULL), tech);
}

void Manager::SetStartupPortalList(const string &portal_list) {
  startup_portal_list_ = portal_list;
  use_startup_portal_list_ = true;
}

bool Manager::IsServiceEphemeral(const ServiceConstRefPtr &service) const {
  return service->profile() == ephemeral_profile_;
}

bool Manager::IsTechnologyShortDNSTimeoutEnabled(
    Technology::Identifier technology) const {
  return IsTechnologyInList(props_.short_dns_timeout_technologies, technology);
}

bool Manager::IsTechnologyLinkMonitorEnabled(
    Technology::Identifier technology) const {
  return IsTechnologyInList(props_.link_monitor_technologies, technology);
}

bool Manager::IsDefaultProfile(StoreInterface *storage) {
  return profiles_.empty() || storage == profiles_.front()->GetConstStorage();
}

void Manager::OnProfileStorageInitialized(StoreInterface *storage) {
  wifi_provider_->FixupServiceEntries(storage, IsDefaultProfile(storage));
}

DeviceRefPtr Manager::GetEnabledDeviceWithTechnology(
    Technology::Identifier technology) const {
  vector<DeviceRefPtr> devices;
  FilterByTechnology(technology, &devices);
  for (vector<DeviceRefPtr>::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if ((*it)->enabled()) {
      return *it;
    }
  }
  return NULL;
}

const ProfileRefPtr &Manager::ActiveProfile() const {
  DCHECK_NE(profiles_.size(), 0U);
  return profiles_.back();
}

bool Manager::IsActiveProfile(const ProfileRefPtr &profile) const {
  return (profiles_.size() > 0 &&
          ActiveProfile().get() == profile.get());
}

bool Manager::MoveServiceToProfile(const ServiceRefPtr &to_move,
                                   const ProfileRefPtr &destination) {
  const ProfileRefPtr from = to_move->profile();
  SLOG(Manager, 2) << "Moving service "
                   << to_move->unique_name()
                   << " to profile "
                   << destination->GetFriendlyName()
                   << " from "
                   << from->GetFriendlyName();
  return destination->AdoptService(to_move) && from->AbandonService(to_move);
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

  if (!to_set->profile()) {
    // We are being asked to set the profile property of a service that
    // has never been registered.  Now is a good time to register it.
    RegisterService(to_set);
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

void Manager::UpdateUninitializedTechnologies() {
  Error error;
  adaptor_->EmitStringsChanged(kUninitializedTechnologiesProperty,
                               UninitializedTechnologies(&error));
}

void Manager::RegisterDevice(const DeviceRefPtr &to_manage) {
  LOG(INFO) << "Device " << to_manage->FriendlyName() << " registered.";
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if (to_manage.get() == it->get())
      return;
  }
  devices_.push_back(to_manage);

  LoadDeviceFromProfiles(to_manage);

  // If |to_manage| is new, it needs to be persisted.
  UpdateDevice(to_manage);

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
      UpdateDevice(to_forget);
      to_forget->SetEnabled(false);
      devices_.erase(it);
      EmitDeviceProperties();
      return;
    }
  }
  SLOG(Manager, 2) << __func__ << " unknown device: "
                   << to_forget->UniqueName();
}

void Manager::LoadDeviceFromProfiles(const DeviceRefPtr &device) {
  // We are applying device properties from the DefaultProfile, and adding the
  // union of hidden services in all loaded profiles to the device.
  for (vector<ProfileRefPtr>::iterator it = profiles_.begin();
       it != profiles_.end(); ++it) {
    // Load device configuration, if any exists, as well as hidden services.
    (*it)->ConfigureDevice(device);
  }
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
  adaptor_->EmitStringsChanged(kUninitializedTechnologiesProperty,
                               UninitializedTechnologies(&error));
}

bool Manager::HasService(const ServiceRefPtr &service) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if ((*it)->unique_name() == service->unique_name())
      return true;
  }
  return false;
}

void Manager::RegisterService(const ServiceRefPtr &to_manage) {
  SLOG(Manager, 2) << "Registering service " << to_manage->unique_name();

  MatchProfileWithService(to_manage);

  // Now add to OUR list.
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    CHECK(to_manage->unique_name() != (*it)->unique_name());
  }
  services_.push_back(to_manage);
  SortServices();
}

void Manager::DeregisterService(const ServiceRefPtr &to_forget) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_forget->unique_name() == (*it)->unique_name()) {
      DCHECK(!(*it)->connection());
      (*it)->Unload();
      (*it)->SetProfile(NULL);
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
  (**service_iterator)->SetProfile(NULL);
  *service_iterator = services_.erase(*service_iterator);

  return true;
}

void Manager::UpdateService(const ServiceRefPtr &to_update) {
  CHECK(to_update);
  LOG(INFO) << "Service " << to_update->unique_name() << " updated;"
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

void Manager::UpdateDevice(const DeviceRefPtr &to_update) {
  LOG(INFO) << "Device " << to_update->link_name() << " updated: "
            << (to_update->enabled_persistent() ? "enabled" : "disabled");
  // Saves the device to the topmost profile that accepts it. Normally, this
  // would be the only DefaultProfile at the bottom of the stack except in
  // autotests that push a second test-only DefaultProfile.
  for (vector<ProfileRefPtr>::reverse_iterator rit = profiles_.rbegin();
       rit != profiles_.rend(); ++rit) {
    if ((*rit)->UpdateDevice(to_update)) {
      return;
    }
  }
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

bool Manager::LoadProperties(const scoped_refptr<DefaultProfile> &profile) {
  if (!profile->LoadManagerProperties(&props_)) {
    return false;
  }
  SetIgnoredDNSSearchPaths(props_.ignored_dns_search_paths, NULL);
  return true;
}

void Manager::AddTerminationAction(const string &name,
                                   const base::Closure &start) {
  if (termination_actions_.IsEmpty() && power_manager_.get()) {
    power_manager_->AddSuspendDelayCallback(
        kPowerManagerKey,
        Bind(&Manager::OnSuspendImminent, AsWeakPtr()));
    CHECK(!suspend_delay_registered_);
    suspend_delay_registered_ = power_manager_->RegisterSuspendDelay(
        base::TimeDelta::FromMilliseconds(
            kTerminationActionsTimeoutMilliseconds),
        kSuspendDelayDescription,
        &suspend_delay_id_);
  }
  termination_actions_.Add(name, start);
}

void Manager::TerminationActionComplete(const string &name) {
  termination_actions_.ActionComplete(name);
}

void Manager::RemoveTerminationAction(const string &name) {
  if (termination_actions_.IsEmpty()) {
    return;
  }
  termination_actions_.Remove(name);
  if (termination_actions_.IsEmpty() && power_manager_.get()) {
    if (suspend_delay_registered_) {
      power_manager_->UnregisterSuspendDelay(suspend_delay_id_);
      suspend_delay_registered_ = false;
      suspend_delay_id_ = 0;
    }
    power_manager_->RemoveSuspendDelayCallback(kPowerManagerKey);
  }
}

void Manager::RunTerminationActions(
    const base::Callback<void(const Error &)> &done) {
  LOG(INFO) << "Running termination actions.";
  termination_actions_.Run(kTerminationActionsTimeoutMilliseconds, done);
}

bool Manager::RunTerminationActionsAndNotifyMetrics(
        const base::Callback<void(const Error &)> &done,
        Metrics::TerminationActionReason reason) {
  if (termination_actions_.IsEmpty())
    return false;

  metrics_->NotifyTerminationActionsStarted(reason);
  RunTerminationActions(done);
  return true;
}

int Manager::RegisterDefaultServiceCallback(const ServiceCallback &callback) {
  default_service_callbacks_[++default_service_callback_tag_] = callback;
  return default_service_callback_tag_;
}

void Manager::DeregisterDefaultServiceCallback(int tag) {
  default_service_callbacks_.erase(tag);
}

void Manager::VerifyDestination(const string &certificate,
                                const string &public_key,
                                const string &nonce,
                                const string &signed_data,
                                const string &destination_udn,
                                const ResultBoolCallback &cb,
                                Error *error) {
  error->Populate(Error::kNotImplemented, "Not implemented");
}

void Manager::VerifyAndEncryptData(const string &certificate,
                                   const string &public_key,
                                   const string &nonce,
                                   const string &signed_data,
                                   const string &destination_udn,
                                   const string &data,
                                   const ResultStringCallback &cb,
                                   Error *error) {
  error->Populate(Error::kNotImplemented, "Not implemented");
}

void Manager::VerifyAndEncryptCredentials(const string &certificate,
                                          const string &public_key,
                                          const string &nonce,
                                          const string &signed_data,
                                          const string &destination_udn,
                                          const string &network_path,
                                          const ResultStringCallback &cb,
                                          Error *error) {
  error->Populate(Error::kNotImplemented, "Not implemented");
}

void Manager::NotifyDefaultServiceChanged(const ServiceRefPtr &service) {
  for (map<int, ServiceCallback>::const_iterator it =
           default_service_callbacks_.begin();
       it != default_service_callbacks_.end(); ++it) {
    it->second.Run(service);
  }
  metrics_->NotifyDefaultServiceChanged(service);
  EmitDefaultService();
}

void Manager::EmitDefaultService() {
  RpcIdentifier rpc_identifier = GetDefaultServiceRpcIdentifier(NULL);
  if (rpc_identifier != default_service_rpc_identifier_) {
    adaptor_->EmitRpcIdentifierChanged(shill::kDefaultServiceProperty,
                                       rpc_identifier);
    default_service_rpc_identifier_ = rpc_identifier;
  }
}

void Manager::OnPowerStateChanged(
    PowerManagerProxyDelegate::SuspendState power_state) {
  if (power_state == PowerManagerProxyDelegate::kOn) {
    vector<ServiceRefPtr>::iterator sit;
    for (sit = services_.begin(); sit != services_.end(); ++sit) {
      (*sit)->OnAfterResume();
    }
    SortServices();
    vector<DeviceRefPtr>::iterator it;
    for (it = devices_.begin(); it != devices_.end(); ++it) {
      (*it)->OnAfterResume();
    }
  } else if (power_state == PowerManagerProxyDelegate::kMem) {
    vector<DeviceRefPtr>::iterator it;
    for (it = devices_.begin(); it != devices_.end(); ++it) {
      (*it)->OnBeforeSuspend();
    }
  }
}

void Manager::OnSuspendImminent(int suspend_id) {
  if (!RunTerminationActionsAndNotifyMetrics(
       Bind(&Manager::OnSuspendActionsComplete, AsWeakPtr(), suspend_id),
       Metrics::kTerminationActionReasonSuspend)) {
    LOG(INFO) << "No suspend actions were run.";
    power_manager_->ReportSuspendReadiness(suspend_delay_id_, suspend_id);
  }
}

void Manager::OnSuspendActionsComplete(int suspend_id, const Error &error) {
  LOG(INFO) << "Finished suspend actions.  Result: " << error;
  metrics_->NotifyTerminationActionsCompleted(
      Metrics::kTerminationActionReasonSuspend, error.IsSuccess());
  power_manager_->ReportSuspendReadiness(suspend_delay_id_, suspend_id);
}

void Manager::FilterByTechnology(Technology::Identifier tech,
                                 vector<DeviceRefPtr> *found) const {
  CHECK(found);
  vector<DeviceRefPtr>::const_iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if ((*it)->technology() == tech)
      found->push_back(*it);
  }
}

ServiceRefPtr Manager::FindService(const string &name) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (name == (*it)->unique_name())
      return *it;
  }
  return NULL;
}

void Manager::HelpRegisterConstDerivedRpcIdentifier(
    const string &name,
    RpcIdentifier(Manager::*get)(Error *error)) {
  store_.RegisterDerivedRpcIdentifier(
      name,
      RpcIdentifierAccessor(
          new CustomAccessor<Manager, RpcIdentifier>(this, get, NULL)));
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
  // We might be called in the middle of a series of events that
  // may result in multiple calls to Manager::SortServices, or within
  // an outer loop that may also be traversing the services_ list.
  // Defer this work to the event loop.
  if (sort_services_task_.IsCancelled()) {
    sort_services_task_.Reset(Bind(&Manager::SortServicesTask, AsWeakPtr()));
    dispatcher_->PostTask(sort_services_task_.callback());
  }
}

void Manager::SortServicesTask() {
  SLOG(Manager, 4) << "In " << __func__;
  sort_services_task_.Cancel();
  ServiceRefPtr default_service;

  if (!services_.empty()) {
    // Keep track of the service that is the candidate for the default
    // service.  We have not yet tested to see if this service has a
    // connection.
    default_service = services_[0];
  }
  const bool kCompareConnectivityState = true;
  sort(services_.begin(), services_.end(),
       ServiceSorter(kCompareConnectivityState, technology_order_));

  adaptor_->EmitRpcIdentifierArrayChanged(flimflam::kServicesProperty,
                                          EnumerateAvailableServices(NULL));
  adaptor_->EmitRpcIdentifierArrayChanged(flimflam::kServiceWatchListProperty,
                                          EnumerateWatchedServices(NULL));

  Error error;
  adaptor_->EmitStringsChanged(flimflam::kConnectedTechnologiesProperty,
                               ConnectedTechnologies(&error));
  adaptor_->EmitStringChanged(flimflam::kDefaultTechnologyProperty,
                              DefaultTechnology(&error));

  if (!services_.empty()) {
    ConnectionRefPtr default_connection = default_service->connection();
    if (default_connection &&
        services_[0]->connection() != default_connection) {
      default_connection->SetIsDefault(false);
    }
    if (services_[0]->connection()) {
      services_[0]->connection()->SetIsDefault(true);
      if (default_service != services_[0]) {
        default_service = services_[0];
        LOG(INFO) << "Default service is now "
                  << default_service->unique_name();
      }
    } else {
      default_service = NULL;
    }
  }
  NotifyDefaultServiceChanged(default_service);
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
  if (!running_) {
    LOG(INFO) << "Auto-connect suppressed -- not running.";
    return;
  }
  if (power_manager_.get() &&
      power_manager_->power_state() != PowerManagerProxyDelegate::kOn &&
      power_manager_->power_state() != PowerManagerProxyDelegate::kUnknown) {
    LOG(INFO) << "Auto-connect suppressed -- power state is not 'on'.";
    return;
  }
  if (services_.empty()) {
    LOG(INFO) << "Auto-connect suppressed -- no services.";
    return;
  }

  if (SLOG_IS_ON(Manager, 4)) {
    SLOG(Manager, 4) << "Sorted service list: ";
    for (size_t i = 0; i < services_.size(); ++i) {
      ServiceRefPtr service = services_[i];
      const char *compare_reason = NULL;
      if (i + 1 < services_.size()) {
        const bool kCompareConnectivityState = true;
        Service::Compare(
            service, services_[i+1], kCompareConnectivityState,
            technology_order_, &compare_reason);
      } else {
        compare_reason = "last";
      }
      SLOG(Manager, 4) << "Service " << service->unique_name()
                       << " IsConnected: " << service->IsConnected()
                       << " IsConnecting: " << service->IsConnecting()
                       << " IsFailed: " << service->IsFailed()
                       << " connectable: " << service->connectable()
                       << " auto_connect: " << service->auto_connect()
                       << " favorite: " << service->favorite()
                       << " priority: " << service->priority()
                       << " crypto_algorithm: " << service->crypto_algorithm()
                       << " key_rotation: " << service->key_rotation()
                       << " endpoint_auth: " << service->endpoint_auth()
                       << " strength: " << service->strength()
                       << " sorted: " << compare_reason;
    }
  }

  // Perform auto-connect.
  for (vector<ServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    if ((*it)->auto_connect()) {
      (*it)->AutoConnect();
    }
  }
}

void Manager::ConnectToBestServices(Error */*error*/) {
  dispatcher_->PostTask(Bind(&Manager::ConnectToBestServicesTask, AsWeakPtr()));
}

void Manager::ConnectToBestServicesTask() {
  vector<ServiceRefPtr> services_copy = services_;
  const bool kCompareConnectivityState = false;
  sort(services_copy.begin(), services_copy.end(),
       ServiceSorter(kCompareConnectivityState, technology_order_));
  set<Technology::Identifier> connecting_technologies;
  for (vector<ServiceRefPtr>::const_iterator it = services_copy.begin();
       it != services_copy.end();
       ++it) {
    if (!(*it)->connectable()) {
      // Due to service sort order, it is guaranteed that no services beyond
      // this one will be connectable either.
      break;
    }
    if (!(*it)->auto_connect()) {
      continue;
    }
    Technology::Identifier technology = (*it)->technology();
    if (!Technology::IsPrimaryConnectivityTechnology(technology) &&
        !IsOnline()) {
      // Non-primary services need some other service connected first.
      continue;
    }
    if (ContainsKey(connecting_technologies, technology)) {
      // We have already started a connection for this technology.
      continue;
    }
    connecting_technologies.insert(technology);
    if (!(*it)->IsConnected() && !(*it)->IsConnecting()) {
      // At first blush, it may seem that using Service::AutoConnect might
      // be the right choice, however Service::IsAutoConnectable and its
      // overridden implementations consider a host of conditions which
      // prevent it from attempting a connection which we'd like to ignore
      // for the purposes of this user-initiated action.
      Error error;
      (*it)->Connect(&error);
      if (error.IsFailure()) {
        LOG(ERROR) << "Connection failed: " << error.message();
      }
    }
  }
}

bool Manager::IsOnline() const {
  // |services_| is sorted such that connected services are first.
  return !services_.empty() && services_.front()->IsConnected();
}

string Manager::CalculateState(Error */*error*/) {
  return IsOnline() ? flimflam::kStateOnline : flimflam::kStateOffline;
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

bool Manager::IsTechnologyConnected(Technology::Identifier technology) const {
  for (vector<DeviceRefPtr>::const_iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if ((*it)->technology() == technology && (*it)->IsConnected())
      return true;
  }
  return false;
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

vector<string> Manager::UninitializedTechnologies(Error */*error*/) {
  return device_info_.GetUninitializedTechnologies();
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
    if ((*it)->IsVisible()) {
      service_rpc_ids.push_back((*it)->GetRpcIdentifier());
    }
  }
  return service_rpc_ids;
}

RpcIdentifiers Manager::EnumerateCompleteServices(Error */*error*/) {
  vector<string> service_rpc_ids;
  for (vector<ServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    service_rpc_ids.push_back((*it)->GetRpcIdentifier());
  }
  return service_rpc_ids;
}

RpcIdentifiers Manager::EnumerateWatchedServices(Error */*error*/) {
  vector<string> service_rpc_ids;
  for (vector<ServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    if ((*it)->IsVisible() && (*it)->IsActive(NULL)) {
      service_rpc_ids.push_back((*it)->GetRpcIdentifier());
    }
  }
  return service_rpc_ids;
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

string Manager::GetIgnoredDNSSearchPaths(Error */*error*/) {
  return props_.ignored_dns_search_paths;
}

void Manager::SetIgnoredDNSSearchPaths(const string &ignored_paths,
                                       Error */*error*/) {
  props_.ignored_dns_search_paths = ignored_paths;
  vector<string> ignored_path_list;
  if (!ignored_paths.empty()) {
    base::SplitString(ignored_paths, ',', &ignored_path_list);
  }
  resolver_->set_ignored_search_list(ignored_path_list);
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

  ServiceRefPtr service = GetServiceInner(args, error);
  if (service) {
    // Configures the service using the rest of the passed-in arguments.
    service->Configure(args, error);
  }

  return service;
}

ServiceRefPtr Manager::GetServiceInner(const KeyValueStore &args,
                                       Error *error) {
  if (args.ContainsString(flimflam::kGuidProperty)) {
    SLOG(Manager, 2) << __func__ << ": searching by GUID";
    ServiceRefPtr service =
        GetServiceWithGUID(args.GetString(flimflam::kGuidProperty), NULL);
    if (service) {
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
    return vpn_provider_->GetService(args, error);
  }
  if (type == flimflam::kTypeWifi) {
    SLOG(Manager, 2) << __func__ << ": getting WiFi Service";
    return wifi_provider_->GetService(args, error);
  }
  if (type == flimflam::kTypeWimax) {
    SLOG(Manager, 2) << __func__ << ": getting WiMAX Service";
    return wimax_provider_.GetService(args, error);
  }
  Error::PopulateAndLog(error, Error::kNotSupported,
                        kErrorUnsupportedServiceType);
  return NULL;
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

  // First pull in any stored configuration associated with the service.
  if (service->profile() == profile) {
    SLOG(Manager, 2) << __func__ << ": service " << service->unique_name()
                     << " is already a member of profile "
                     << profile->GetFriendlyName()
                     << " so a load is not necessary.";
  } else if (profile->LoadService(service)) {
    SLOG(Manager, 2) << __func__ << ": applied stored information from profile "
                     << profile->GetFriendlyName()
                     << " into service "
                     << service->unique_name();
  } else {
    SLOG(Manager, 2) << __func__ << ": no previous information in profile "
                     << profile->GetFriendlyName()
                     << " exists for service "
                     << service->unique_name();
  }

  // Overlay this with the passed-in configuration parameters.
  service->Configure(args, error);

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

ServiceRefPtr Manager::FindMatchingService(const KeyValueStore &args,
                                           Error *error) {
  for (vector<ServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    if ((*it)->DoPropertiesMatch(args)) {
      return *it;
    }
  }
  error->Populate(Error::kNotFound, "Matching service was not found");
  return NULL;
}

map<string, GeolocationInfos> Manager::GetNetworksForGeolocation() {
  map<string, GeolocationInfos> networks;
  for (vector<DeviceRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    switch((*it)->technology()) {
      // TODO(gauravsh): crosbug.com/35736 Need a strategy for combining
      // geolocation objects from multiple devices of the same technolgy.
      // Currently, we just pick the geolocation objects from the first found
      // device of each supported technology type.
      case Technology::kWifi:
        if (!ContainsKey(networks, kGeoWifiAccessPointsProperty))
          networks[kGeoWifiAccessPointsProperty] =
              (*it)->GetGeolocationObjects();
        break;
      case Technology::kCellular:
        if (!ContainsKey(networks, kGeoCellTowersProperty))
          networks[kGeoCellTowersProperty] = (*it)->GetGeolocationObjects();
        break;
      default:
        // Ignore other technologies.
        break;
    };
  }
  return networks;
};

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
