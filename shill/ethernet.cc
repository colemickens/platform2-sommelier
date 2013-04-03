// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet.h"

#include <time.h>
#include <stdio.h>
#include <netinet/ether.h>
#include <linux/if.h>  // Needs definitions from netinet/ether.h

#include <string>

#include <base/bind.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/eap_listener.h"
#include "shill/ethernet_service.h"
#include "shill/event_dispatcher.h"
#include "shill/logging.h"
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
                   const string &address,
                   int interface_index)
    : Device(control_interface,
             dispatcher,
             metrics,
             manager,
             link_name,
             address,
             interface_index,
             Technology::kEthernet),
      link_up_(false),
      is_eap_detected_(false),
      eap_listener_(new EapListener(dispatcher, interface_index)) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstBool(kEapAuthenticatorDetectedProperty,
                           &is_eap_detected_);
  // Unretained() is okay here since eap_listener_ is owned by |this|.
  eap_listener_->set_request_received_callback(
      base::Bind(&Ethernet::OnEapDetected, base::Unretained(this)));
  SLOG(Ethernet, 2) << "Ethernet device " << link_name << " initialized.";
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
  rtnl_handler()->SetInterfaceFlags(interface_index(), IFF_UP, IFF_UP);
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

void Ethernet::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_LOWER_UP) != 0 && !link_up_) {
    link_up_ = true;
    if (service_) {
      LOG(INFO) << "Registering " << link_name() << " with manager.";
      // Manager will bring up L3 for us.
      manager()->RegisterService(service_);
    }
    eap_listener_->Start();
  } else if ((flags & IFF_LOWER_UP) == 0 && link_up_) {
    link_up_ = false;
    is_eap_detected_ = false;
    DestroyIPConfig();
    if (service_)
      manager()->DeregisterService(service_);
    SelectService(NULL);
    eap_listener_->Stop();
  }
}

void Ethernet::ConnectTo(EthernetService *service) {
  CHECK(service == service_.get()) << "Ethernet was asked to connect the "
                                   << "wrong service?";
  if (AcquireIPConfigWithLeaseName(service->GetStorageIdentifier())) {
    SelectService(service);
    SetServiceState(Service::kStateConfiguring);
  } else {
    LOG(ERROR) << "Unable to acquire DHCP config.";
    SetServiceState(Service::kStateFailure);
  }
}

void Ethernet::DisconnectFrom(EthernetService *service) {
  CHECK(service == service_.get()) << "Ethernet was asked to disconnect the "
                                   << "wrong service?";
  DropConnection();
}

void Ethernet::OnEapDetected() {
  is_eap_detected_ = true;
  eap_listener_->Stop();
}

}  // namespace shill
