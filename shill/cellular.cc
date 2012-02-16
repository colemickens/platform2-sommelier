// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <netinet/in.h>
#include <linux/if.h>  // Needs definitions from netinet/in.h

#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular_capability_cdma.h"
#include "shill/cellular_capability_gsm.h"
#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/technology.h"

using base::Bind;
using std::map;
using std::string;
using std::vector;

namespace shill {

Cellular::Operator::Operator() {
  SetName("");
  SetCode("");
  SetCountry("");
}

Cellular::Operator::~Operator() {}

void Cellular::Operator::CopyFrom(const Operator &oper) {
  dict_ = oper.dict_;
}

const string &Cellular::Operator::GetName() const {
  return dict_.find(flimflam::kOperatorNameKey)->second;
}

void Cellular::Operator::SetName(const string &name) {
  dict_[flimflam::kOperatorNameKey] = name;
}

const string &Cellular::Operator::GetCode() const {
  return dict_.find(flimflam::kOperatorCodeKey)->second;
}

void Cellular::Operator::SetCode(const string &code) {
  dict_[flimflam::kOperatorCodeKey] = code;
}

const string &Cellular::Operator::GetCountry() const {
  return dict_.find(flimflam::kOperatorCountryKey)->second;
}

void Cellular::Operator::SetCountry(const string &country) {
  dict_[flimflam::kOperatorCountryKey] = country;
}

const Stringmap &Cellular::Operator::ToDict() const {
  return dict_;
}

Cellular::Cellular(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Metrics *metrics,
                   Manager *manager,
                   const string &link_name,
                   const string &address,
                   int interface_index,
                   Type type,
                   const string &owner,
                   const string &path,
                   mobile_provider_db *provider_db)
    : Device(control_interface,
             dispatcher,
             metrics,
             manager,
             link_name,
             address,
             interface_index,
             Technology::kCellular),
      state_(kStateDisabled),
      modem_state_(kModemStateUnknown),
      dbus_owner_(owner),
      dbus_path_(path),
      provider_db_(provider_db) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kDBusConnectionProperty, &dbus_owner_);
  store->RegisterConstString(flimflam::kDBusObjectProperty, &dbus_path_);
  store->RegisterConstStringmap(flimflam::kHomeProviderProperty,
                                &home_provider_.ToDict());
  // For now, only a single capability is supported.
  InitCapability(type, ProxyFactory::GetInstance());

  VLOG(2) << "Cellular device " << this->link_name() << " initialized.";
}

Cellular::~Cellular() {
}

// static
string Cellular::GetStateString(State state) {
  switch (state) {
    case kStateDisabled: return "CellularStateDisabled";
    case kStateEnabled: return "CellularStateEnabled";
    case kStateRegistered: return "CellularStateRegistered";
    case kStateConnected: return "CellularStateConnected";
    case kStateLinked: return "CellularStateLinked";
    default: NOTREACHED();
  }
  return StringPrintf("CellularStateUnknown-%d", state);
}

void Cellular::SetState(State state) {
  VLOG(2) << GetStateString(state_) << " -> " << GetStateString(state);
  state_ = state;
}

void Cellular::Start() {
  LOG(INFO) << __func__ << ": " << GetStateString(state_);
  if (state_ != kStateDisabled) {
    return;
  }
  Device::Start();
  if (modem_state_ == kModemStateEnabled) {
    // Modem already enabled.
    OnModemEnabled();
    return;
  }
  // TODO(ers): this should not be done automatically. It should
  // require an explicit Enable request to start the modem.
  capability_->StartModem();
}

void Cellular::Stop() {
  DestroyService();
  DisconnectModem();
  if (state_ != kStateDisabled)
    capability_->DisableModem(NULL);
  capability_->StopModem();
  Device::Stop();
}

void Cellular::InitCapability(Type type, ProxyFactory *proxy_factory) {
  // TODO(petkov): Consider moving capability construction into a factory that's
  // external to the Cellular class.
  VLOG(2) << __func__ << "(" << type << ")";
  switch (type) {
    case kTypeGSM:
      capability_.reset(new CellularCapabilityGSM(this, proxy_factory));
      break;
    case kTypeCDMA:
      capability_.reset(new CellularCapabilityCDMA(this, proxy_factory));
      break;
    default: NOTREACHED();
  }
}

void Cellular::OnModemEnabled() {
  VLOG(2) << __func__ << ": " << GetStateString(state_);
  if (state_ == kStateDisabled) {
    SetState(kStateEnabled);
  }
}

void Cellular::OnModemDisabled() {
  VLOG(2) << __func__ << ": " << GetStateString(state_);
  if (state_ != kStateDisabled) {
    SetState(kStateDisabled);
  }
}

void Cellular::Activate(const std::string &carrier,
                        ReturnerInterface *returner) {
  capability_->Activate(carrier, new AsyncCallHandler(returner));
}

void Cellular::RegisterOnNetwork(const string &network_id,
                                 ReturnerInterface *returner) {
  capability_->RegisterOnNetwork(network_id, new AsyncCallHandler(returner));
}

void Cellular::RequirePIN(
    const string &pin, bool require, ReturnerInterface *returner) {
  VLOG(2) << __func__ << "(" << returner << ")";
  capability_->RequirePIN(pin, require, new AsyncCallHandler(returner));
}

void Cellular::EnterPIN(const string &pin, ReturnerInterface *returner) {
  VLOG(2) << __func__ << "(" << returner << ")";
  capability_->EnterPIN(pin, new AsyncCallHandler(returner));
}

void Cellular::UnblockPIN(const string &unblock_code,
                          const string &pin,
                          ReturnerInterface *returner) {
  VLOG(2) << __func__ << "(" << returner << ")";
  capability_->UnblockPIN(unblock_code, pin, new AsyncCallHandler(returner));
}

void Cellular::ChangePIN(
    const string &old_pin, const string &new_pin,
    ReturnerInterface *returner) {
  VLOG(2) << __func__ << "(" << returner << ")";
  capability_->ChangePIN(old_pin, new_pin, new AsyncCallHandler(returner));
}

void Cellular::Scan(Error * /*error*/) {
  // TODO(ers): for now report immediate success.
  capability_->Scan(NULL);
}

void Cellular::HandleNewRegistrationState() {
  dispatcher()->PostTask(Bind(&Cellular::HandleNewRegistrationStateTask, this));
}

void Cellular::HandleNewRegistrationStateTask() {
  VLOG(2) << __func__;
  if (!capability_->IsRegistered()) {
    DestroyService();
    if (state_ == kStateLinked ||
        state_ == kStateConnected ||
        state_ == kStateRegistered) {
      SetState(kStateEnabled);
    }
    return;
  }
  if (state_ == kStateEnabled) {
    SetState(kStateRegistered);
  }
  if (!service_.get()) {
    CreateService();
  }
  capability_->GetSignalQuality();
  if (state_ == kStateRegistered && modem_state_ == kModemStateConnected)
    OnConnected();
  service_->SetNetworkTechnology(capability_->GetNetworkTechnologyString());
  service_->SetRoamingState(capability_->GetRoamingStateString());
}

void Cellular::HandleNewSignalQuality(uint32 strength) {
  VLOG(2) << "Signal strength: " << strength;
  if (service_) {
    service_->SetStrength(strength);
  }
}

void Cellular::CreateService() {
  VLOG(2) << __func__;
  CHECK(!service_.get());
  service_ =
      new CellularService(control_interface(), dispatcher(), metrics(),
                          manager(), this);
  capability_->OnServiceCreated();
  manager()->RegisterService(service_);
}

void Cellular::DestroyService() {
  VLOG(2) << __func__;
  DestroyIPConfig();
  if (service_) {
    manager()->DeregisterService(service_);
    service_ = NULL;
  }
  SelectService(NULL);
}

bool Cellular::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kCellular;
}

void Cellular::Connect(Error *error) {
  VLOG(2) << __func__;
  if (state_ == kStateConnected ||
      state_ == kStateLinked) {
    Error::PopulateAndLog(error, Error::kAlreadyConnected,
                          "Already connected; connection request ignored.");
    return;
  }
  CHECK_EQ(kStateRegistered, state_);

  if (!capability_->allow_roaming() &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    Error::PopulateAndLog(error, Error::kNotOnHomeNetwork,
                          "Roaming disallowed; connection request ignored.");
    return;
  }

  DBusPropertiesMap properties;
  capability_->SetupConnectProperties(&properties);
  capability_->Connect(properties);
}

void Cellular::OnConnected() {
  VLOG(2) << __func__;
  SetState(kStateConnected);
  if (!capability_->allow_roaming() &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    Disconnect(NULL);
  } else {
    EstablishLink();
  }
}

void Cellular::OnConnectFailed() {
  // TODO(ers): Signal failure.
}

void Cellular::Disconnect(Error *error) {
  VLOG(2) << __func__;
  if (state_ != kStateConnected &&
      state_ != kStateLinked) {
    Error::PopulateAndLog(
        error, Error::kInProgress, "Not connected; request ignored.");
    return;
  }
  capability_->Disconnect();
}

void Cellular::OnDisconnected() {
  VLOG(2) << __func__;
  SetState(kStateRegistered);
}

void Cellular::DisconnectModem() {
  VLOG(2) << __func__;
  if (state_ != kStateConnected &&
      state_ != kStateLinked) {
    return;
  }
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583). Note,
  // however, that this is invoked by the Stop method, so some extra refactoring
  // may be needed to prevent the device instance from being destroyed before
  // the modem manager calls are complete. Maybe Disconnect and Disable need to
  // be handled by the parent Modem class.
  capability_->Disconnect();
}

void Cellular::OnDisconnectFailed() {
  // TODO(ers): Signal failure.
}

void Cellular::EstablishLink() {
  VLOG(2) << __func__;
  CHECK_EQ(kStateConnected, state_);
  unsigned int flags = 0;
  if (manager()->device_info()->GetFlags(interface_index(), &flags) &&
      (flags & IFF_UP) != 0) {
    LinkEvent(flags, IFF_UP);
    return;
  }
  // TODO(petkov): Provide a timeout for a failed link-up request.
  rtnl_handler()->SetInterfaceFlags(interface_index(), IFF_UP, IFF_UP);
}

void Cellular::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_UP) != 0 && state_ == kStateConnected) {
    LOG(INFO) << link_name() << " is up.";
    SetState(kStateLinked);
    // TODO(petkov): For GSM, remember the APN.
    if (AcquireIPConfig()) {
      SelectService(service_);
      SetServiceState(Service::kStateConfiguring);
    } else {
      LOG(ERROR) << "Unable to acquire DHCP config.";
    }
  } else if ((flags & IFF_UP) == 0 && state_ == kStateLinked) {
    SetState(kStateConnected);
    DestroyService();
  }
}

void Cellular::OnModemManagerPropertiesChanged(
    const DBusPropertiesMap &properties) {
  capability_->OnModemManagerPropertiesChanged(properties);
}

void Cellular::set_home_provider(const Operator &oper) {
  home_provider_.CopyFrom(oper);
}

string Cellular::CreateFriendlyServiceName() {
  VLOG(2) << __func__;
  return capability_.get() ? capability_->CreateFriendlyServiceName() : "";
}

}  // namespace shill
