// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <string>

#include <base/logging.h>
#include <base/memory/ref_counted.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/manager.h"
#include "shill/dbus_adaptor.h"
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

void Manager::RegisterDevice(Device *to_manage) {
  vector<scoped_refptr<Device> >::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_manage == it->get())
      return;
  }
  devices_.push_back(scoped_refptr<Device>(to_manage));

  // TODO(pstew): Should check configuration
  if (running_)
    to_manage->Start();
}

void Manager::DeregisterDevice(const Device *to_forget) {
  vector<scoped_refptr<Device> >::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if (to_forget == it->get()) {
      devices_.erase(it);
      return;
    }
  }
}

void Manager::RegisterService(Service *to_manage) {
  vector<scoped_refptr<Service> >::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_manage == it->get())
      return;
  }
  services_.push_back(scoped_refptr<Service>(to_manage));
}

void Manager::DeregisterService(const Service *to_forget) {
  vector<scoped_refptr<Service> >::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (to_forget == it->get()) {
      services_.erase(it);
      return;
    }
  }
}

void Manager::FilterByTechnology(Device::Technology tech,
                                 vector<scoped_refptr<Device> > *found) {
  CHECK(found);
  vector<scoped_refptr<Device> >::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it) {
    if ((*it)->TechnologyIs(tech))
      found->push_back(*it);
  }
}

Service* Manager::FindService(const std::string& name) {
  vector<scoped_refptr<Service> >::iterator it;
  for (it = services_.begin(); it != services_.end(); ++it) {
    if (name == (*it)->UniqueName())
      return it->get();
  }
}

}  // namespace shill
