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
  CHECK(profiles_[0]->InitStorage(glib_));

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
    (*it)->Finalize();
  }
  ephemeral_profile_->Finalize();

  adaptor_->UpdateRunning();
  modem_info_.Stop();
  device_info_.Stop();
}

void Manager::AdoptProfile(const ProfileRefPtr &profile) {
  profiles_.push_back(profile);
}

const ProfileRefPtr &Manager::ActiveProfile() {
  DCHECK_NE(profiles_.size(), 0);
  return profiles_.back();
}

bool Manager::MoveToActiveProfile(const ProfileRefPtr &from,
                                  const ServiceRefPtr &to_move) {
  return ActiveProfile()->AdoptService(to_move) &&
      from->AbandonService(to_move->UniqueName());
}

void Manager::RegisterDevice(const DeviceRefPtr &to_manage) {
  // TODO(pstew): Should DefaultProfile have a list of devices, analogous to
  // the list of services that it manages?  If so, we should do a similar merge
  // thing here.

  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_manage.get() == it->get())
      return;
  }
  devices_.push_back(to_manage);

  // TODO(pstew): Should check configuration
  if (running_)
    to_manage->Start();
}

void Manager::DeregisterDevice(const DeviceRefPtr &to_forget) {
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_forget.get() == it->get()) {
      VLOG(2) << "Deregistered device: " << to_forget->UniqueName();
      to_forget->Stop();
      devices_.erase(it);
      return;
    }
  }
  VLOG(2) << __func__ << " unknown device: " << to_forget->UniqueName();
}

void Manager::RegisterService(const ServiceRefPtr &to_manage) {
  VLOG(2) << __func__ << to_manage->UniqueName();

  for (vector<ProfileRefPtr>::iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    if ((*it)->MergeService(to_manage))  // this will merge, if possible.
      break;
  }

  // If not found, add it to the ephemeral profile
  ephemeral_profile_->AdoptService(to_manage);

  // Now add to OUR list.
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    CHECK(to_manage->UniqueName() != (*it)->UniqueName());
  }
  services_.push_back(to_manage);
  SortServices();

  vector<string> service_paths;
  for (it = services_.begin(); it != services_.end(); ++it) {
    service_paths.push_back((*it)->GetRpcIdentifier());
  }
  adaptor_->EmitRpcIdentifierArrayChanged(flimflam::kServicesProperty,
                                          service_paths);
}

void Manager::DeregisterService(const ServiceConstRefPtr &to_forget) {
  // If the service is in the ephemeral profile, destroy it.
  if (!ephemeral_profile_->AbandonService(to_forget->UniqueName())) {
    // if it's in one of the real profiles...um...I guess mark it unconnectable?
  }
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_forget->UniqueName() == (*it)->UniqueName()) {
      services_.erase(it);
      SortServices();
      return;
    }
  }
}

void Manager::UpdateService(const ServiceConstRefPtr &to_update) {
  LOG(INFO) << "Service " << to_update->UniqueName() << " updated;"
            << " state: " << to_update->state() << " failure: "
            << to_update->failure();
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
    string(Manager::*get)(void),
    void(Manager::*set)(const string&, Error *)) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Manager, string>(this, get, set)));
}

void Manager::HelpRegisterDerivedStrings(
    const string &name,
    Strings(Manager::*get)(void),
    void(Manager::*set)(const Strings &, Error *)) {
  store_.RegisterDerivedStrings(
      name,
      StringsAccessor(new CustomAccessor<Manager, Strings>(this, get, set)));
}

void Manager::SortServices() {
  sort(services_.begin(), services_.end(), ServiceSorter(technology_order_));
}

string Manager::CalculateState() {
  return flimflam::kStateOffline;
}

vector<string> Manager::AvailableTechnologies() {
  return vector<string>();
}

vector<string> Manager::ConnectedTechnologies() {
  return vector<string>();
}

string Manager::DefaultTechnology() {
  return "";
}

vector<string> Manager::EnabledTechnologies() {
  return vector<string>();
}

vector<string> Manager::EnumerateDevices() {
  vector<string> device_rpc_ids;
  for (vector<DeviceRefPtr>::const_iterator it = devices_.begin();
       it != devices_.end();
       ++it) {
    device_rpc_ids.push_back((*it)->GetRpcIdentifier());
  }
  return device_rpc_ids;
}

vector<string> Manager::EnumerateAvailableServices() {
  vector<string> service_rpc_ids;
  for (vector<ServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    service_rpc_ids.push_back((*it)->GetRpcIdentifier());
  }
  return service_rpc_ids;
}

vector<string> Manager::EnumerateWatchedServices() {
  // TODO(cmasone): Filter this list for services in appropriate states.
  return EnumerateAvailableServices();
}

string Manager::GetActiveProfileName() {
  return ActiveProfile()->GetFriendlyName();
}

// called via RPC (e.g., from ManagerDBusAdaptor)
WiFiServiceRefPtr Manager::GetWifiService(const KeyValueStore &args,
                                          Error *error) {
  std::vector<DeviceRefPtr> wifi_devices;
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
    const string kMessage = "Unrecognized technology " + technology;
    LOG(ERROR) << kMessage;
    CHECK(error);
    error->Populate(Error::kInvalidArguments, kMessage);
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
      error->Populate(Error::kInvalidArguments, *it +
                      " is an unknown technology name");
      return;
    }

    if (ContainsKey(seen, identifier)) {
      error->Populate(Error::kInvalidArguments, *it +
                      " is duplicated in the list");
      return;
    }
    seen[identifier] = true;
    new_order.push_back(identifier);
  }

  technology_order_ = new_order;
  SortServices();
}

}  // namespace shill
