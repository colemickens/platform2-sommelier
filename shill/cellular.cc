// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <netinet/in.h>
#include <linux/if.h>

#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/manager.h"
#include "shill/modem_simple_proxy_interface.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/shill_event.h"

using std::make_pair;
using std::string;
using std::vector;

namespace shill {

const char Cellular::kConnectPropertyPhoneNumber[] = "number";
const char Cellular::kPhoneNumberCDMA[] = "#777";
const char Cellular::kPhoneNumberGSM[] = "*99#";

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

Cellular::Network::Network() {
  dict_[flimflam::kStatusProperty] = "";
  dict_[flimflam::kNetworkIdProperty] = "";
  dict_[flimflam::kShortNameProperty] = "";
  dict_[flimflam::kLongNameProperty] = "";
  dict_[flimflam::kTechnologyProperty] = "";
}

Cellular::Network::~Network() {}

const string &Cellular::Network::GetStatus() const {
  return dict_.find(flimflam::kStatusProperty)->second;
}

void Cellular::Network::SetStatus(const string &status) {
  dict_[flimflam::kStatusProperty] = status;
}

const string &Cellular::Network::GetId() const {
  return dict_.find(flimflam::kNetworkIdProperty)->second;
}

void Cellular::Network::SetId(const string &id) {
  dict_[flimflam::kNetworkIdProperty] = id;
}

const string &Cellular::Network::GetShortName() const {
  return dict_.find(flimflam::kShortNameProperty)->second;
}

void Cellular::Network::SetShortName(const string &name) {
  dict_[flimflam::kShortNameProperty] = name;
}

const string &Cellular::Network::GetLongName() const {
  return dict_.find(flimflam::kLongNameProperty)->second;
}

void Cellular::Network::SetLongName(const string &name) {
  dict_[flimflam::kLongNameProperty] = name;
}

const string &Cellular::Network::GetTechnology() const {
  return dict_.find(flimflam::kTechnologyProperty)->second;
}

void Cellular::Network::SetTechnology(const string &technology) {
  dict_[flimflam::kTechnologyProperty] = technology;
}

const Stringmap &Cellular::Network::ToDict() const {
  return dict_;
}

Cellular::CDMA::CDMA()
    : registration_state_evdo(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      registration_state_1x(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      activation_state(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED),
      prl_version(0) {}

Cellular::GSM::GSM()
    : registration_state(MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN),
      access_technology(MM_MODEM_GSM_ACCESS_TECH_UNKNOWN) {}

Cellular::Cellular(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager,
                   const string &link_name,
                   const string &address,
                   int interface_index,
                   Type type,
                   const string &owner,
                   const string &path)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             address,
             interface_index),
      type_(type),
      state_(kStateDisabled),
      modem_state_(kModemStateUnknown),
      dbus_owner_(owner),
      dbus_path_(path),
      task_factory_(this),
      allow_roaming_(false),
      scanning_(false),
      scan_interval_(0) {
  PropertyStore *store = this->store();
  store->RegisterConstString(flimflam::kCarrierProperty, &carrier_);
  store->RegisterConstString(flimflam::kDBusConnectionProperty, &dbus_owner_);
  store->RegisterConstString(flimflam::kDBusObjectProperty, &dbus_path_);
  store->RegisterBool(flimflam::kCellularAllowRoamingProperty, &allow_roaming_);
  store->RegisterConstString(flimflam::kEsnProperty, &esn_);
  store->RegisterConstString(flimflam::kFirmwareRevisionProperty,
                             &firmware_revision_);
  store->RegisterConstString(flimflam::kHardwareRevisionProperty,
                             &hardware_revision_);
  store->RegisterConstStringmap(flimflam::kHomeProviderProperty,
                                &home_provider_.ToDict());
  store->RegisterConstString(flimflam::kImeiProperty, &imei_);
  store->RegisterConstString(flimflam::kImsiProperty, &imsi_);
  store->RegisterConstString(flimflam::kManufacturerProperty, &manufacturer_);
  store->RegisterConstString(flimflam::kMdnProperty, &mdn_);
  store->RegisterConstString(flimflam::kMeidProperty, &meid_);
  store->RegisterConstString(flimflam::kMinProperty, &min_);
  store->RegisterConstString(flimflam::kModelIDProperty, &model_id_);
  store->RegisterConstUint16(flimflam::kPRLVersionProperty, &cdma_.prl_version);
  store->RegisterConstString(flimflam::kSelectedNetworkProperty,
                             &selected_network_);

  HelpRegisterDerivedStrIntPair(flimflam::kSIMLockStatusProperty,
                                &Cellular::SimLockStatusToProperty,
                                NULL);
  HelpRegisterDerivedStringmaps(flimflam::kFoundNetworksProperty,
                                &Cellular::EnumerateNetworks,
                                NULL);

  store->RegisterConstBool(flimflam::kScanningProperty, &scanning_);
  store->RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);

  VLOG(2) << "Cellular device " << this->link_name() << " initialized: "
          << GetTypeString();
}

Cellular::~Cellular() {}

string Cellular::GetTypeString() const {
  switch (type_) {
    case kTypeGSM: return "CellularTypeGSM";
    case kTypeCDMA: return "CellularTypeCDMA";
    default: NOTREACHED();
  }
  return StringPrintf("CellularTypeUnknown-%d", type_);
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

string Cellular::GetNetworkTechnologyString() const {
  switch (type_) {
    case kTypeGSM:
      if (gsm_.registration_state == MM_MODEM_GSM_NETWORK_REG_STATUS_HOME ||
          gsm_.registration_state == MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING) {
        switch (gsm_.access_technology) {
          case MM_MODEM_GSM_ACCESS_TECH_GSM:
          case MM_MODEM_GSM_ACCESS_TECH_GSM_COMPACT:
            return flimflam::kNetworkTechnologyGsm;
          case MM_MODEM_GSM_ACCESS_TECH_GPRS:
            return flimflam::kNetworkTechnologyGprs;
          case MM_MODEM_GSM_ACCESS_TECH_EDGE:
            return flimflam::kNetworkTechnologyEdge;
          case MM_MODEM_GSM_ACCESS_TECH_UMTS:
            return flimflam::kNetworkTechnologyUmts;
          case MM_MODEM_GSM_ACCESS_TECH_HSDPA:
          case MM_MODEM_GSM_ACCESS_TECH_HSUPA:
          case MM_MODEM_GSM_ACCESS_TECH_HSPA:
            return flimflam::kNetworkTechnologyHspa;
          case MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS:
            return flimflam::kNetworkTechnologyHspaPlus;
          default:
            NOTREACHED();
        }
      }
      break;
    case kTypeCDMA:
      if (cdma_.registration_state_evdo !=
          MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
        return flimflam::kNetworkTechnologyEvdo;
      }
      if (cdma_.registration_state_1x !=
          MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
        return flimflam::kNetworkTechnology1Xrtt;
      }
      break;
    default:
      NOTREACHED();
  }
  return "";
}

string Cellular::GetRoamingStateString() const {
  switch (type_) {
    case kTypeGSM:
      switch (gsm_.registration_state) {
        case MM_MODEM_GSM_NETWORK_REG_STATUS_HOME:
          return flimflam::kRoamingStateHome;
        case MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING:
          return flimflam::kRoamingStateRoaming;
        default:
          break;
      }
      break;
    case kTypeCDMA: {
      uint32 state = cdma_.registration_state_evdo;
      if (state == MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
        state = cdma_.registration_state_1x;
      }
      switch (state) {
        case MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN:
        case MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED:
          break;
        case MM_MODEM_CDMA_REGISTRATION_STATE_HOME:
          return flimflam::kRoamingStateHome;
        case MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING:
          return flimflam::kRoamingStateRoaming;
        default:
          NOTREACHED();
      }
      break;
    }
    default:
      NOTREACHED();
  }
  return flimflam::kRoamingStateUnknown;
}

// static
string Cellular::GetCDMAActivationStateString(uint32 state) {
  switch (state) {
    case MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED:
      return flimflam::kActivationStateActivated;
    case MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING:
      return flimflam::kActivationStateActivating;
    case MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED:
      return flimflam::kActivationStateNotActivated;
    case MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED:
      return flimflam::kActivationStatePartiallyActivated;
    default:
      return flimflam::kActivationStateUnknown;
  }
}

// static
string Cellular::GetCDMAActivationErrorString(uint32 error) {
  switch (error) {
    case MM_MODEM_CDMA_ACTIVATION_ERROR_WRONG_RADIO_INTERFACE:
      return flimflam::kErrorNeedEvdo;
    case MM_MODEM_CDMA_ACTIVATION_ERROR_ROAMING:
      return flimflam::kErrorNeedHomeNetwork;
    case MM_MODEM_CDMA_ACTIVATION_ERROR_COULD_NOT_CONNECT:
    case MM_MODEM_CDMA_ACTIVATION_ERROR_SECURITY_AUTHENTICATION_FAILED:
    case MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED:
      return flimflam::kErrorOtaspFailed;
    case MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR:
      return "";
    case MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL:
    default:
      return flimflam::kErrorActivationFailed;
  }
}

void Cellular::SetState(State state) {
  VLOG(2) << GetStateString(state_) << " -> " << GetStateString(state);
  state_ = state;
}

void Cellular::Start() {
  LOG(INFO) << __func__ << ": " << GetStateString(state_);
  Device::Start();
  InitProxies();
  EnableModem();
  if (type_ == kTypeGSM) {
    RegisterGSMModem();
  }
  GetModemStatus();
  GetModemIdentifiers();
  if (type_ == kTypeGSM) {
    GetGSMProperties();
  }
  GetModemInfo();
  GetModemRegistrationState();
}

void Cellular::Stop() {
  proxy_.reset();
  simple_proxy_.reset();
  cdma_proxy_.reset();
  manager()->DeregisterService(service_);
  service_ = NULL;  // Breaks a reference cycle.
  SelectService(NULL);
  SetState(kStateDisabled);
  RTNLHandler::GetInstance()->SetInterfaceFlags(interface_index(), 0, IFF_UP);
  Device::Stop();
}

void Cellular::InitProxies() {
  VLOG(2) << __func__;
  proxy_.reset(
      ProxyFactory::factory()->CreateModemProxy(this, dbus_path_, dbus_owner_));
  simple_proxy_.reset(
      ProxyFactory::factory()->CreateModemSimpleProxy(
          dbus_path_, dbus_owner_));
  switch (type_) {
    case kTypeGSM:
      gsm_network_proxy_.reset(
          ProxyFactory::factory()->CreateModemGSMNetworkProxy(
              this, dbus_path_, dbus_owner_));
      break;
    case kTypeCDMA:
      cdma_proxy_.reset(
          ProxyFactory::factory()->CreateModemCDMAProxy(
              this, dbus_path_, dbus_owner_));
      break;
    default: NOTREACHED();
  }
}

void Cellular::EnableModem() {
  CHECK_EQ(kStateDisabled, state_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  proxy_->Enable(true);
  SetState(kStateEnabled);
}

void Cellular::GetModemStatus() {
  CHECK_EQ(kStateEnabled, state_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  DBusPropertiesMap properties = simple_proxy_->GetStatus();
  if (DBusProperties::GetString(properties, "carrier", &carrier_) &&
      type_ == kTypeCDMA) {
    home_provider_.SetName(carrier_);
    home_provider_.SetCode("");
    home_provider_.SetCountry("us");
  }
  DBusProperties::GetString(properties, "meid", &meid_);
  DBusProperties::GetString(properties, "imei", &imei_);
  if (DBusProperties::GetString(properties, "imsi", &imsi_) &&
      type_ == kTypeGSM) {
    // TODO(petkov): Set GSM provider.
  }
  DBusProperties::GetString(properties, "esn", &esn_);
  DBusProperties::GetString(properties, "mdn", &mdn_);
  DBusProperties::GetString(properties, "min", &min_);
  DBusProperties::GetString(
      properties, "firmware_revision", &firmware_revision_);

  uint32 state = 0;
  if (DBusProperties::GetUint32(properties, "state", &state)) {
    modem_state_ = static_cast<ModemState>(state);
  }

  if (type_ == kTypeCDMA) {
    DBusProperties::GetUint32(
        properties, "activation_state", &cdma_.activation_state);
    DBusProperties::GetUint16(properties, "prl_version", &cdma_.prl_version);
    // TODO(petkov): For now, get the payment and usage URLs from ModemManager
    // to match flimflam. In the future, provide a plugin API to get these
    // directly from the modem driver.
    DBusProperties::GetString(properties, "payment_url", &cdma_.payment_url);
    DBusProperties::GetString(properties, "usage_url", &cdma_.usage_url);
  }
}

void Cellular::GetModemIdentifiers() {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void Cellular::GetGSMProperties() {
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  gsm_.access_technology = gsm_network_proxy_->AccessTechnology();
  VLOG(2) << "GSM AccessTechnology: " << gsm_.access_technology;
}

void Cellular::RegisterGSMModem() {
  LOG(INFO) << "Registering on: "
            << (selected_network_.empty() ? "home network" : selected_network_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  gsm_network_proxy_->Register(selected_network_);
}

void Cellular::GetModemInfo() {
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  ModemProxyInterface::Info info = proxy_->GetInfo();
  manufacturer_ = info._1;
  model_id_ = info._2;
  hardware_revision_ = info._3;
  VLOG(2) << "ModemInfo: " << manufacturer_ << ", " << model_id_ << ", "
          << hardware_revision_;
}

void Cellular::GetModemRegistrationState() {
  switch (type_) {
    case kTypeGSM:
      GetGSMRegistrationState();
      break;
    case kTypeCDMA:
      GetCDMARegistrationState();
      break;
    default: NOTREACHED();
  }
  HandleNewRegistrationState();
}

void Cellular::GetCDMARegistrationState() {
  CHECK_EQ(kTypeCDMA, type_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  cdma_proxy_->GetRegistrationState(&cdma_.registration_state_1x,
                                    &cdma_.registration_state_evdo);
  VLOG(2) << "CDMA Registration: 1x(" << cdma_.registration_state_1x
          << ") EVDO(" << cdma_.registration_state_evdo << ")";
}

void Cellular::GetGSMRegistrationState() {
  CHECK_EQ(kTypeGSM, type_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  ModemGSMNetworkProxyInterface::RegistrationInfo info =
      gsm_network_proxy_->GetRegistrationInfo();
  gsm_.registration_state = info._1;
  gsm_.network_id = info._2;
  gsm_.operator_name = info._3;
  VLOG(2) << "GSM Registration: " << gsm_.registration_state << ", "
          << gsm_.network_id << ", "  << gsm_.operator_name;
}

void Cellular::HandleNewRegistrationState() {
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &Cellular::HandleNewRegistrationStateTask));
}

void Cellular::HandleNewRegistrationStateTask() {
  VLOG(2) << __func__;
  const string network_tech = GetNetworkTechnologyString();
  if (network_tech.empty()) {
    if (state_ == kStateLinked) {
      manager()->DeregisterService(service_);
    }
    service_ = NULL;
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
    // For now, no endpoint is created. Revisit if necessary.
    CreateService();
  }
  GetModemSignalQuality();
  if (state_ == kStateRegistered && modem_state_ == kModemStateConnected) {
    SetState(kStateConnected);
    EstablishLink();
  }
  service_->set_network_tech(network_tech);
  service_->set_roaming_state(GetRoamingStateString());
  // TODO(petkov): For GSM, update the serving operator based on the network id
  // and the mobile provider database.
}

void Cellular::GetModemSignalQuality() {
  VLOG(2) << __func__;
  uint32 strength = 0;
  switch (type_) {
    case kTypeGSM:
      strength = GetGSMSignalQuality();
      break;
    case kTypeCDMA:
      strength = GetCDMASignalQuality();
      break;
    default: NOTREACHED();
  }
  HandleNewSignalQuality(strength);
}

uint32 Cellular::GetCDMASignalQuality() {
  VLOG(2) << __func__;
  CHECK_EQ(kTypeCDMA, type_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  return cdma_proxy_->GetSignalQuality();
}

uint32 Cellular::GetGSMSignalQuality() {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
  return 0;
}

void Cellular::HandleNewSignalQuality(uint32 strength) {
  VLOG(2) << "Signal strength: " << strength;
  if (service_.get()) {
    service_->set_strength(strength);
  }
}

void Cellular::CreateService() {
  VLOG(2) << __func__;
  CHECK(!service_.get());
  service_ =
      new CellularService(control_interface(), dispatcher(), manager(), this);
  switch (type_) {
    case kTypeGSM:
      service_->set_activation_state(flimflam::kActivationStateActivated);
      // TODO(petkov): Set serving operator.
      break;
    case kTypeCDMA:
      service_->set_payment_url(cdma_.payment_url);
      service_->set_usage_url(cdma_.usage_url);
      service_->set_serving_operator(home_provider_);
      HandleNewCDMAActivationState(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR);
      break;
    default:
      NOTREACHED();
  }
}

bool Cellular::TechnologyIs(const Device::Technology type) const {
  return type == Device::kCellular;
}

void Cellular::Connect(Error *error) {
  VLOG(2) << __func__;
  if (state_ == kStateConnected ||
      state_ == kStateLinked) {
    LOG(ERROR) << "Already connected; connection request ignored.";
    CHECK(error);
    error->Populate(Error::kAlreadyConnected);
    return;
  }
  CHECK_EQ(kStateRegistered, state_);

  if (!allow_roaming_ &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    LOG(ERROR) << "Roaming disallowed; connection request ignored.";
    CHECK(error);
    error->Populate(Error::kNotOnHomeNetwork);
    return;
  }

  DBusPropertiesMap properties;
  const char *phone_number = NULL;
  switch (type_) {
    case kTypeGSM:
      phone_number = kPhoneNumberGSM;
      break;
    case kTypeCDMA:
      phone_number = kPhoneNumberCDMA;
      break;
    default: NOTREACHED();
  }
  properties[kConnectPropertyPhoneNumber].writer().append_string(phone_number);
  // TODO(petkov): Setup apn and "home_only".

  // Defer connect because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&Cellular::ConnectTask, properties));
}

void Cellular::ConnectTask(const DBusPropertiesMap &properties) {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  simple_proxy_->Connect(properties);
  SetState(kStateConnected);
  EstablishLink();
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
  RTNLHandler::GetInstance()->SetInterfaceFlags(
      interface_index(), IFF_UP, IFF_UP);
}

void Cellular::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_UP) != 0 && state_ == kStateConnected) {
    LOG(INFO) << link_name() << " is up.";
    SetState(kStateLinked);
    manager()->RegisterService(service_);
    // TODO(petkov): For GSM, remember the APN.
    if (AcquireDHCPConfig()) {
      SelectService(service_);
      SetServiceState(Service::kStateConfiguring);
    } else {
      LOG(ERROR) << "Unable to acquire DHCP config.";
    }
  } else if ((flags & IFF_UP) == 0 && state_ == kStateLinked) {
    SetState(kStateConnected);
    manager()->DeregisterService(service_);
    SelectService(NULL);
    DestroyIPConfig();
  }
}

void Cellular::Activate(const string &carrier, Error *error) {
  if (type_ != kTypeCDMA) {
    const string kMessage = "Unable to activate non-CDMA modem.";
    LOG(ERROR) << kMessage;
    CHECK(error);
    error->Populate(Error::kInvalidArguments, kMessage);
    return;
  }
  if (state_ != kStateEnabled &&
      state_ != kStateRegistered) {
    const string kMessage = "Unable to activate in " + GetStateString(state_);
    LOG(ERROR) << kMessage;
    CHECK(error);
    error->Populate(Error::kInvalidArguments, kMessage);
    return;
  }
  // Defer connect because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&Cellular::ActivateTask, carrier));
}

void Cellular::ActivateTask(const string &carrier) {
  VLOG(2) << __func__ << "(" << carrier << ")";
  if (state_ != kStateEnabled &&
      state_ != kStateRegistered) {
    LOG(ERROR) << "Unable to activate in " << GetStateString(state_);
    return;
  }
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 status = cdma_proxy_->Activate(carrier);
  if (status == MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR) {
    cdma_.activation_state = MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
  }
  HandleNewCDMAActivationState(status);
}

void Cellular::HandleNewCDMAActivationState(uint32 error) {
  if (!service_.get()) {
    return;
  }
  service_->set_activation_state(
      GetCDMAActivationStateString(cdma_.activation_state));
  service_->set_error(GetCDMAActivationErrorString(error));
}

void Cellular::OnCDMAActivationStateChanged(
    uint32 activation_state,
    uint32 activation_error,
    const DBusPropertiesMap &status_changes) {
  CHECK_EQ(kTypeCDMA, type_);
  DBusProperties::GetString(status_changes, "mdn", &mdn_);
  DBusProperties::GetString(status_changes, "min", &min_);
  if (DBusProperties::GetString(
          status_changes, "payment_url", &cdma_.payment_url) &&
      service_.get()) {
    service_->set_payment_url(cdma_.payment_url);
  }
  cdma_.activation_state = activation_state;
  HandleNewCDMAActivationState(activation_error);
}

void Cellular::OnCDMARegistrationStateChanged(uint32 state_1x,
                                              uint32 state_evdo) {
  CHECK_EQ(kTypeCDMA, type_);
  cdma_.registration_state_1x = state_1x;
  cdma_.registration_state_evdo = state_evdo;
  HandleNewRegistrationState();
}

void Cellular::OnCDMASignalQualityChanged(uint32 strength) {
  CHECK_EQ(kTypeCDMA, type_);
  HandleNewSignalQuality(strength);
}

void Cellular::OnGSMNetworkModeChanged(uint32 mode) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void Cellular::OnGSMRegistrationInfoChanged(uint32 status,
                                            const string &operator_code,
                                            const string &operator_name) {
  CHECK_EQ(kTypeGSM, type_);
  gsm_.registration_state = status;
  gsm_.network_id = operator_code;
  gsm_.operator_name = operator_name;
  HandleNewRegistrationState();
}

void Cellular::OnGSMSignalQualityChanged(uint32 quality) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void Cellular::OnModemStateChanged(uint32 old_state,
                                   uint32 new_state,
                                   uint32 reason) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

Stringmaps Cellular::EnumerateNetworks() {
  Stringmaps to_return;
  for (vector<Network>::const_iterator it = found_networks_.begin();
       it != found_networks_.end();
       ++it) {
    to_return.push_back(it->ToDict());
  }
  return to_return;
}

StrIntPair Cellular::SimLockStatusToProperty() {
  return StrIntPair(make_pair(flimflam::kSIMLockTypeProperty,
                              sim_lock_status_.lock_type),
                    make_pair(flimflam::kSIMLockRetriesLeftProperty,
                              sim_lock_status_.retries_left));
}

void Cellular::HelpRegisterDerivedStringmaps(
    const string &name,
    Stringmaps(Cellular::*get)(void),
    bool(Cellular::*set)(const Stringmaps&)) {
  store()->RegisterDerivedStringmaps(
      name,
      StringmapsAccessor(
          new CustomAccessor<Cellular, Stringmaps>(this, get, set)));
}

void Cellular::HelpRegisterDerivedStrIntPair(
    const string &name,
    StrIntPair(Cellular::*get)(void),
    bool(Cellular::*set)(const StrIntPair&)) {
  store()->RegisterDerivedStrIntPair(
      name,
      StrIntPairAccessor(
          new CustomAccessor<Cellular, StrIntPair>(this, get, set)));
}

}  // namespace shill
