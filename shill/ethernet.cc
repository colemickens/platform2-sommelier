// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet.h"

#include <time.h>
#include <stdio.h>
#include <netinet/ether.h>
#include <linux/if.h>  // Needs definitions from netinet/ether.h

#include <string>

#include <base/logging.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/ethernet_service.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/rtnl_handler.h"

using std::string;

namespace shill {

Ethernet::Ethernet(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Metrics *metrics,
                   Manager *manager,
                   const string &link_name,
                   const std::string &address,
                   int interface_index)
    : Device(control_interface,
             dispatcher,
             metrics,
             manager,
             link_name,
             address,
             interface_index,
             Technology::kEthernet),
      service_registered_(false),
      link_up_(false) {
  VLOG(2) << "Ethernet device " << link_name << " initialized.";
}

Ethernet::~Ethernet() {
  Stop(NULL, EnabledStateChangedCallback());
}

void Ethernet::Start(Error *error,
                     const EnabledStateChangedCallback &callback) {
  service_ = new EthernetService(control_interface(),
                                 dispatcher(),
                                 metrics(),
                                 manager(),
                                 this);
  RTNLHandler::GetInstance()->SetInterfaceFlags(interface_index(), IFF_UP,
                                                IFF_UP);
  OnEnabledStateChanged(EnabledStateChangedCallback(), Error());
  if (error)
    error->Reset();       // indicate immediate completion
}

void Ethernet::Stop(Error *error, const EnabledStateChangedCallback &callback) {
  if (service_) {
    manager()->DeregisterService(service_);
    service_ = NULL;
  }
  OnEnabledStateChanged(EnabledStateChangedCallback(), Error());
  if (error)
    error->Reset();       // indicate immediate completion
}

bool Ethernet::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kEthernet;
}

void Ethernet::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_LOWER_UP) != 0 && !link_up_) {
    LOG(INFO) << link_name() << " is up; should start L3!";
    link_up_ = true;
    if (service_) {
      manager()->RegisterService(service_);
      if (service_->auto_connect()) {
        if (AcquireIPConfig()) {
          SelectService(service_);
          SetServiceState(Service::kStateConfiguring);
        } else {
          LOG(ERROR) << "Unable to acquire DHCP config.";
        }
      }
    }
  } else if ((flags & IFF_LOWER_UP) == 0 && link_up_) {
    link_up_ = false;
    DestroyIPConfig();
    manager()->DeregisterService(service_);
    SelectService(NULL);
  }
}

}  // namespace shill
