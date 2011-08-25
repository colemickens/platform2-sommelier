// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <time.h>
#include <stdio.h>

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/default_profile.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/ephemeral_profile.h"
#include "shill/error.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/resolver.h"
#include "shill/shill_event.h"
#include "shill/service.h"

using std::string;
using std::vector;

namespace shill {

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
    ephemeral_profile_(new EphemeralProfile(control_interface, glib, this)),
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
  profiles_.push_back(new DefaultProfile(control_interface_,
                                         glib_,
                                         this,
                                         storage_path_,
                                         props_));
  VLOG(2) << "Manager initialized.";
}

Manager::~Manager() {
  vector<ProfileRefPtr>::iterator it;
  for (it = profiles_.begin(); it != profiles_.end(); ++it) {
    (*it)->Finalize();
  }
  ephemeral_profile_->Finalize();
}

void Manager::AddDeviceToBlackList(const string &device_name) {
  device_info_.AddDeviceToBlackList(device_name);
}

void Manager::Start() {
  LOG(INFO) << "Manager started.";

  CHECK(file_util::CreateDirectory(run_path_)) << run_path_.value();
  Resolver::GetInstance()->set_path(run_path_.Append("resolv.conf"));

  CHECK(file_util::CreateDirectory(storage_path_)) << storage_path_.value();

  running_ = true;
  adaptor_->UpdateRunning();
  device_info_.Start();
  modem_info_.Start();
}

void Manager::Stop() {
  running_ = false;
  adaptor_->UpdateRunning();
  modem_info_.Stop();
  device_info_.Stop();
}

const ProfileRefPtr &Manager::ActiveProfile() {
  return profiles_.back();
}

bool Manager::MoveToActiveProfile(const ProfileRefPtr &from,
                                  const ServiceRefPtr &to_move) {
  return ActiveProfile()->AdoptService(to_move) &&
      from->AbandonService(to_move->UniqueName());
}

void Manager::RegisterDevice(const DeviceRefPtr &to_manage) {
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
  // This should look for |to_manage| in the real profiles and, if found,
  // do...something...to merge the meaningful state, I guess.

  // If not found, add it to the ephemeral profile
  ephemeral_profile_->AdoptService(to_manage);

  // Now add to OUR list.
  // TODO(cmasone): Keep this list sorted.
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_manage->UniqueName() == (*it)->UniqueName())
      return;
  }
  services_.push_back(to_manage);
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
      return;
    }
  }
}

void Manager::UpdateService(const ServiceConstRefPtr &to_update) {
  LOG(INFO) << "Service " << to_update->UniqueName() << " updated;"
            << " state: " << to_update->state() << " failure: "
            << to_update->failure();
  // TODO(pstew): This should trigger re-sorting of services, autoconnect, etc.
}

void Manager::FilterByTechnology(Device::Technology tech,
                                 vector<DeviceRefPtr> *found) {
  CHECK(found);
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if ((*it)->TechnologyIs(tech))
      found->push_back(*it);
  }
}

ServiceRefPtr Manager::FindService(const std::string& name) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (name == (*it)->UniqueName())
      return *it;
  }
  return NULL;
}

void Manager::HelpRegisterDerivedString(const string &name,
                                    string(Manager::*get)(void),
                                    bool(Manager::*set)(const string&)) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Manager, string>(this, get, set)));
}

void Manager::HelpRegisterDerivedStrings(const string &name,
                                     Strings(Manager::*get)(void),
                                     bool(Manager::*set)(const Strings&)) {
  store_.RegisterDerivedStrings(
      name,
      StringsAccessor(new CustomAccessor<Manager, Strings>(this, get, set)));
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

}  // namespace shill
