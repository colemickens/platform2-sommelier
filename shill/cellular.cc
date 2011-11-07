// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <netinet/in.h>
#include <linux/if.h>  // Needs definitions from netinet/in.h

#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular_capability_cdma.h"
#include "shill/cellular_capability_gsm.h"
#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/modem_simple_proxy_interface.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"

using std::make_pair;
using std::map;
using std::string;
using std::vector;

namespace shill {

const char Cellular::kConnectPropertyPhoneNumber[] = "number";
const char Cellular::kNetworkPropertyAccessTechnology[] = "access-tech";
const char Cellular::kNetworkPropertyID[] = "operator-num";
const char Cellular::kNetworkPropertyLongName[] = "operator-long";
const char Cellular::kNetworkPropertyShortName[] = "operator-short";
const char Cellular::kNetworkPropertyStatus[] = "status";
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
                   const string &path,
                   mobile_provider_db *provider_db)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             address,
             interface_index),
      proxy_factory_(ProxyFactory::GetInstance()),
      type_(type),
      state_(kStateDisabled),
      modem_state_(kModemStateUnknown),
      dbus_owner_(owner),
      dbus_path_(path),
      provider_db_(provider_db),
      task_factory_(this),
      allow_roaming_(false),
      scanning_(false),
      scan_interval_(0) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kCarrierProperty, &carrier_);
  store->RegisterConstString(flimflam::kDBusConnectionProperty, &dbus_owner_);
  store->RegisterConstString(flimflam::kDBusObjectProperty, &dbus_path_);
  store->RegisterBool(flimflam::kCellularAllowRoamingProperty, &allow_roaming_);
  store->RegisterConstString(flimflam::kEsnProperty, &esn_);
  store->RegisterConstString(flimflam::kFirmwareRevisionProperty,
                             &firmware_revision_);
  store->RegisterConstStringmaps(flimflam::kFoundNetworksProperty,
                                 &found_networks_);
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
  InitCapability();  // For now, only a single capability is supported.
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
  capability_.reset();
  proxy_.reset();
  simple_proxy_.reset();
  cdma_proxy_.reset();
  gsm_card_proxy_.reset();
  gsm_network_proxy_.reset();
  manager()->DeregisterService(service_);
  service_ = NULL;  // Breaks a reference cycle.
  SetState(kStateDisabled);
  Device::Stop();
}

void Cellular::InitCapability() {
  VLOG(2) << __func__;
  switch (type_) {
    case kTypeGSM:
      capability_.reset(new CellularCapabilityGSM(this));
      break;
    case kTypeCDMA:
      capability_.reset(new CellularCapabilityCDMA(this));
      break;
    default: NOTREACHED();
  }
}

void Cellular::InitProxies() {
  VLOG(2) << __func__;
  proxy_.reset(proxy_factory_->CreateModemProxy(this, dbus_path_, dbus_owner_));
  simple_proxy_.reset(
      proxy_factory_->CreateModemSimpleProxy(dbus_path_, dbus_owner_));
  capability_->InitProxies();
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
    // TODO(petkov): Set GSM provider based on IMSI and SPN.
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
  VLOG(2) << __func__;
  switch (type_) {
    case kTypeGSM:
      GetGSMIdentifiers();
      break;
    case kTypeCDMA:
      GetCDMAIdentifiers();
      break;
    default: NOTREACHED();
  }
}

void Cellular::GetCDMAIdentifiers() {
  CHECK_EQ(kTypeCDMA, type_);
  if (meid_.empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    meid_ = cdma_proxy_->MEID();
    VLOG(2) << "MEID: " << imei_;
  }
}

void Cellular::GetGSMIdentifiers() {
  CHECK_EQ(kTypeGSM, type_);
  if (imei_.empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    imei_ = gsm_card_proxy_->GetIMEI();
    VLOG(2) << "IMEI: " << imei_;
  }
  if (imsi_.empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    imsi_ = gsm_card_proxy_->GetIMSI();
    VLOG(2) << "IMSI: " << imsi_;
  }
  if (gsm_.spn.empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    try {
      gsm_.spn = gsm_card_proxy_->GetSPN();
      VLOG(2) << "SPN: " << gsm_.spn;
    } catch (const DBus::Error e) {
      // Some modems don't support this call so catch the exception explicitly.
      LOG(WARNING) << "Unable to obtain SPN: " << e.what();
    }
  }
  if (mdn_.empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    mdn_ = gsm_card_proxy_->GetMSISDN();
    VLOG(2) << "MSISDN/MDN: " << mdn_;
  }
}

void Cellular::GetGSMProperties() {
  CHECK_EQ(kTypeGSM, type_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  gsm_.access_technology = gsm_network_proxy_->AccessTechnology();
  VLOG(2) << "GSM AccessTechnology: " << gsm_.access_technology;
}

void Cellular::RegisterGSMModem() {
  CHECK_EQ(kTypeGSM, type_);
  LOG(INFO) << "Registering on \"" << selected_network_ << "\"";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  gsm_network_proxy_->Register(selected_network_);
  // TODO(petkov): Handle registration failure including trying the home network
  // when selected_network_ is not empty.
}

void Cellular::RegisterOnNetwork(const string &network_id, Error *error) {
  LOG(INFO) << __func__ << "(" << network_id << ")";
  if (type_ != kTypeGSM) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          "Network registration supported only for GSM.");
    return;
  }
  // Defer registration because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&Cellular::RegisterOnNetworkTask,
                                      network_id));
}

void Cellular::RegisterOnNetworkTask(const string &network_id) {
  LOG(INFO) << __func__ << "(" << network_id << ")";
  CHECK_EQ(kTypeGSM, type_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  gsm_network_proxy_->Register(network_id);
  // TODO(petkov): Handle registration failure.
  selected_network_ = network_id;
}

void Cellular::RequirePIN(const string &pin, bool require, Error *error) {
  capability_->RequirePIN(pin, require, error);
}

void Cellular::EnterPIN(const string &pin, Error *error) {
  capability_->EnterPIN(pin, error);
}

void Cellular::UnblockPIN(
    const string &unblock_code, const string &pin, Error *error) {
  capability_->UnblockPIN(unblock_code, pin, error);
}

void Cellular::ChangePIN(
    const string &old_pin, const string &new_pin, Error *error) {
  capability_->ChangePIN(old_pin, new_pin, error);
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
  UpdateGSMOperatorInfo();
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
  VLOG(2) << __func__;
  CHECK_EQ(kTypeGSM, type_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  return gsm_network_proxy_->GetSignalQuality();
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
      UpdateServingOperator();
      break;
    case kTypeCDMA:
      service_->set_payment_url(cdma_.payment_url);
      service_->set_usage_url(cdma_.usage_url);
      UpdateServingOperator();
      HandleNewCDMAActivationState(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR);
      break;
    default:
      NOTREACHED();
  }
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

  if (!allow_roaming_ &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    Error::PopulateAndLog(error, Error::kNotOnHomeNetwork,
                          "Roaming disallowed; connection request ignored.");
    CHECK(error);
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
  rtnl_handler()->SetInterfaceFlags(interface_index(), IFF_UP, IFF_UP);
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

void Cellular::Scan(Error *error) {
  VLOG(2) << __func__;
  if (type_ != kTypeGSM) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          "Network scanning support for GSM only.");
    return;
  }
  // Defer scan because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&Cellular::ScanTask));
}

void Cellular::ScanTask() {
  VLOG(2) << __func__;
  // TODO(petkov): Defer scan requests if a scan is in progress already.
  CHECK_EQ(kTypeGSM, type_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583). This is a
  // must for this call which is basically a stub at this point.
  ModemGSMNetworkProxyInterface::ScanResults results =
      gsm_network_proxy_->Scan();
  found_networks_.clear();
  for (ModemGSMNetworkProxyInterface::ScanResults::const_iterator it =
           results.begin(); it != results.end(); ++it) {
    found_networks_.push_back(ParseScanResult(*it));
  }
}

Stringmap Cellular::ParseScanResult(
    const ModemGSMNetworkProxyInterface::ScanResult &result) {
  Stringmap parsed;
  for (ModemGSMNetworkProxyInterface::ScanResult::const_iterator it =
           result.begin(); it != result.end(); ++it) {
    // TODO(petkov): Define these in system_api/service_constants.h. The
    // numerical values are taken from 3GPP TS 27.007 Section 7.3.
    static const char * const kStatusString[] = {
      "unknown",
      "available",
      "current",
      "forbidden",
    };
    static const char * const kTechnologyString[] = {
      flimflam::kNetworkTechnologyGsm,
      "GSM Compact",
      flimflam::kNetworkTechnologyUmts,
      flimflam::kNetworkTechnologyEdge,
      "HSDPA",
      "HSUPA",
      flimflam::kNetworkTechnologyHspa,
    };
    VLOG(2) << "Network property: " << it->first << " = " << it->second;
    if (it->first == kNetworkPropertyStatus) {
      int status = 0;
      if (base::StringToInt(it->second, &status) &&
          status >= 0 &&
          status < static_cast<int>(arraysize(kStatusString))) {
        parsed[flimflam::kStatusProperty] = kStatusString[status];
      } else {
        LOG(ERROR) << "Unexpected status value: " << it->second;
      }
    } else if (it->first == kNetworkPropertyID) {
      parsed[flimflam::kNetworkIdProperty] = it->second;
    } else if (it->first == kNetworkPropertyLongName) {
      parsed[flimflam::kLongNameProperty] = it->second;
    } else if (it->first == kNetworkPropertyShortName) {
      parsed[flimflam::kShortNameProperty] = it->second;
    } else if (it->first == kNetworkPropertyAccessTechnology) {
      int tech = 0;
      if (base::StringToInt(it->second, &tech) &&
          tech >= 0 &&
          tech < static_cast<int>(arraysize(kTechnologyString))) {
        parsed[flimflam::kTechnologyProperty] = kTechnologyString[tech];
      } else {
        LOG(ERROR) << "Unexpected technology value: " << it->second;
      }
    } else {
      LOG(WARNING) << "Unknown network property ignored: " << it->first;
    }
  }
  // If the long name is not available but the network ID is, look up the long
  // name in the mobile provider database.
  if ((!ContainsKey(parsed, flimflam::kLongNameProperty) ||
       parsed[flimflam::kLongNameProperty].empty()) &&
      ContainsKey(parsed, flimflam::kNetworkIdProperty)) {
    mobile_provider *provider =
        mobile_provider_lookup_by_network(
            provider_db_, parsed[flimflam::kNetworkIdProperty].c_str());
    if (provider) {
      const char *long_name = mobile_provider_get_name(provider);
      if (long_name && *long_name) {
        parsed[flimflam::kLongNameProperty] = long_name;
      }
    }
  }
  return parsed;
}

void Cellular::Activate(const string &carrier, Error *error) {
  VLOG(2) << __func__ << "(" << carrier << ")";
  if (type_ != kTypeCDMA) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Unable to activate non-CDMA modem.");
    return;
  }
  if (state_ != kStateEnabled &&
      state_ != kStateRegistered) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Unable to activate in " + GetStateString(state_));
    return;
  }
  // Defer connect because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&Cellular::ActivateTask, carrier));
}

void Cellular::ActivateTask(const string &carrier) {
  VLOG(2) << __func__ << "(" << carrier << ")";
  CHECK_EQ(kTypeCDMA, type_);
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

void Cellular::OnGSMNetworkModeChanged(uint32 /*mode*/) {
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
  UpdateGSMOperatorInfo();
  HandleNewRegistrationState();
}

void Cellular::OnGSMSignalQualityChanged(uint32 quality) {
  CHECK_EQ(kTypeGSM, type_);
  HandleNewSignalQuality(quality);
}

void Cellular::OnModemStateChanged(uint32 /*old_state*/,
                                   uint32 /*new_state*/,
                                   uint32 /*reason*/) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void Cellular::SetGSMAccessTechnology(uint32 access_technology) {
  CHECK_EQ(kTypeGSM, type_);
  gsm_.access_technology = access_technology;
  if (service_.get()) {
    service_->set_network_tech(GetNetworkTechnologyString());
  }
}

void Cellular::UpdateGSMOperatorInfo() {
  if (!gsm_.network_id.empty()) {
    VLOG(2) << "Looking up network id: " << gsm_.network_id;
    mobile_provider *provider =
        mobile_provider_lookup_by_network(provider_db_,
                                          gsm_.network_id.c_str());
    if (provider) {
      const char *provider_name = mobile_provider_get_name(provider);
      if (provider_name && *provider_name) {
        gsm_.operator_name = provider_name;
        gsm_.operator_country = provider->country;
        VLOG(2) << "Operator name: " << gsm_.operator_name
                << ", country: " << gsm_.operator_country;
      }
    } else {
      VLOG(2) << "GSM provider not found.";
    }
  }
  UpdateServingOperator();
}

void Cellular::UpdateServingOperator() {
  if (!service_.get()) {
    return;
  }
  switch (type_) {
    case kTypeGSM: {
      Operator oper;
      oper.SetName(gsm_.operator_name);
      oper.SetCode(gsm_.network_id);
      oper.SetCountry(gsm_.operator_country);
      service_->set_serving_operator(oper);
      break;
    }
    case kTypeCDMA:
      service_->set_serving_operator(home_provider_);
      break;
    default: NOTREACHED();
  }
}

StrIntPair Cellular::SimLockStatusToProperty() {
  return StrIntPair(make_pair(flimflam::kSIMLockTypeProperty,
                              sim_lock_status_.lock_type),
                    make_pair(flimflam::kSIMLockRetriesLeftProperty,
                              sim_lock_status_.retries_left));
}

void Cellular::HelpRegisterDerivedStrIntPair(
    const string &name,
    StrIntPair(Cellular::*get)(void),
    void(Cellular::*set)(const StrIntPair&, Error *)) {
  mutable_store()->RegisterDerivedStrIntPair(
      name,
      StrIntPairAccessor(
          new CustomAccessor<Cellular, StrIntPair>(this, get, set)));
}

}  // namespace shill
