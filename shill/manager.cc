// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <time.h>
#include <stdio.h>

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/default_profile.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/shill_event.h"
#include "shill/service.h"

using std::string;
using std::vector;

namespace shill {
Manager::Manager(ControlInterface *control_interface,
                 EventDispatcher *dispatcher)
  : adaptor_(control_interface->CreateManagerAdaptor(this)),
    device_info_(control_interface, dispatcher, this),
    running_(false) {
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

  profiles_.push_back(new DefaultProfile(control_interface, &glib_, props_));

  VLOG(2) << "Manager initialized.";
}

Manager::~Manager() {}

void Manager::Start() {
  LOG(INFO) << "Manager started.";
  running_ = true;
  adaptor_->UpdateRunning();
  device_info_.Start();
}

void Manager::Stop() {
  running_ = false;
  adaptor_->UpdateRunning();
}

const ProfileRefPtr &Manager::ActiveProfile() {
  return profiles_.back();
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

void Manager::DeregisterDevice(const DeviceConstRefPtr &to_forget) {
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_forget.get() == it->get()) {
      devices_.erase(it);
      return;
    }
  }
}

void Manager::RegisterService(const ServiceRefPtr &to_manage) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_manage.get() == it->get())
      return;
  }
  services_.push_back(to_manage);
}

void Manager::DeregisterService(const ServiceConstRefPtr &to_forget) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_forget.get() == it->get()) {
      services_.erase(it);
      return;
    }
  }
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
    if (name == (*it)->UniqueName()) {
      return *it;
    }
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
  // TODO(cmasone): This should, instead, be returned by calling into the
  // currently active profile.
  vector<string> service_rpc_ids;
  for (vector<ServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    service_rpc_ids.push_back((*it)->GetRpcIdentifier());
  }
  return service_rpc_ids;
}

vector<string> Manager::EnumerateWatchedServices() {
  // TODO(cmasone): Implement this for real by querying the active profile.
  return EnumerateAvailableServices();
}

string Manager::GetActiveProfileName() {
  return ActiveProfile()->name();
}

}  // namespace shill
