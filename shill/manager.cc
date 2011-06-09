// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <time.h>
#include <stdio.h>

#include <string>

#include <base/logging.h>
#include <base/memory/ref_counted.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
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
  // Initialize Interface monitor, so we can detect new interfaces
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

void Manager::RegisterDevice(DeviceRefPtr to_manage) {
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

void Manager::DeregisterDevice(DeviceConstRefPtr to_forget) {
  vector<DeviceRefPtr>::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_forget.get() == it->get()) {
      devices_.erase(it);
      return;
    }
  }
}

void Manager::RegisterService(ServiceRefPtr to_manage) {
  vector<ServiceRefPtr>::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_manage.get() == it->get())
      return;
  }
  services_.push_back(to_manage);
}

void Manager::DeregisterService(ServiceConstRefPtr to_forget) {
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

bool Manager::SetBoolProperty(const string& name, bool value, Error *error) {
  VLOG(2) << "Setting " << name << " as a bool.";
  // TODO(cmasone): Set actual properties.
  return true;
}

bool Manager::SetStringProperty(const string& name,
                                const string& value,
                                Error *error) {
  VLOG(2) << "Setting " << name << " as a string.";
  // TODO(cmasone): Set actual properties.
  return true;
}

}  // namespace shill
