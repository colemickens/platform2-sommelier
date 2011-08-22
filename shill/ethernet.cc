// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet.h"

#include <time.h>
#include <stdio.h>
#include <netinet/ether.h>
#include <linux/if.h>

#include <string>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/manager.h"
#include "shill/shill_event.h"
#include "shill/ethernet_service.h"
#include "shill/profile.h"
#include "shill/rtnl_handler.h"

using std::string;

namespace shill {

Ethernet::Ethernet(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager,
                   const string &link_name,
                   const std::string &address,
                   int interface_index)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             address,
             interface_index),
      service_registered_(false),
      service_(new EthernetService(control_interface,
                                   dispatcher,
                                   manager,
                                   this)),
      link_up_(false) {
  VLOG(2) << "Ethernet device " << link_name << " initialized.";
}

Ethernet::~Ethernet() {
  Stop();
}

void Ethernet::Start() {
  Device::Start();
  RTNLHandler::GetInstance()->SetInterfaceFlags(interface_index_, IFF_UP,
                                                IFF_UP);
}

void Ethernet::Stop() {
  manager_->DeregisterService(service_);
  DestroyIPConfig();
  Device::Stop();
  RTNLHandler::GetInstance()->SetInterfaceFlags(interface_index_, 0, IFF_UP);
}

bool Ethernet::TechnologyIs(const Device::Technology type) const {
  return type == Device::kEthernet;
}

void Ethernet::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_LOWER_UP) != 0 && !link_up_) {
    LOG(INFO) << link_name_ << " is up; should start L3!";
    link_up_ = true;
    manager_->RegisterService(service_);
    if (service_->auto_connect()) {
      if (AcquireDHCPConfig()) {
        SelectService(service_);
        SetServiceState(Service::kStateConfiguring);
      } else {
        LOG(ERROR) << "Unable to acquire DHCP config.";
      }
    }
  } else if ((flags & IFF_LOWER_UP) == 0 && link_up_) {
    link_up_ = false;
    manager_->DeregisterService(service_);
    SelectService(NULL);
    DestroyIPConfig();
  }
}

}  // namespace shill
