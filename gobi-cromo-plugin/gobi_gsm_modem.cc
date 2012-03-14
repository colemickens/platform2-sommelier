// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_gsm_modem.h"

#include "gobi_modem_handler.h"
#include <base/scoped_ptr.h>
#include <cromo/carrier.h>
#include <cromo/sms_message.h>
#include <mm/mm-modem.h>

#include <ctype.h> // for isspace(), to implement TrimWhitespaceASCII()
#include <stdio.h> // for sscanf()
#include <sstream>

using cromo::SmsMessage;
using cromo::SmsMessageFragment;

static const uint32_t kPinRetriesNotKnown = 999;

//======================================================================
// Construct and destruct
GobiGsmModem::~GobiGsmModem() {
}

//======================================================================
// Callbacks and callback utilities

void GobiGsmModem::SignalStrengthHandler(INT8 signal_strength,
                                         ULONG radio_interface) {
  unsigned long ss_percent = MapDbmToPercent(signal_strength);

  // TODO(ers) make sure radio interface corresponds to the network
  // on which we're registered
  SignalQuality(ss_percent);  // NB:  org.freedesktop...Modem.Gsm.Network

  // See whether we're going from no signal to signal. If so, that's an
  // indication that we're now registered on a network, so get registration
  // info and send it out.
  if (!signal_available_) {
    signal_available_ = true;
    RegistrationStateHandler();
  }
}

void GobiGsmModem::RegistrationStateHandler() {
  uint32_t registration_status;
  std::string operator_code;
  std::string operator_name;
  DBus::Error error;

  LOG(INFO) << "RegistrationStateHandler";
  GetGsmRegistrationInfo(&registration_status,
                         &operator_code, &operator_name, error);
  if (!error) {
    MMModemState mm_modem_state;
    RegistrationInfo(registration_status, operator_code, operator_name);
    switch (registration_status) {
      case MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE:
      case MM_MODEM_GSM_NETWORK_REG_STATUS_DENIED:
        mm_modem_state = MM_MODEM_STATE_ENABLED;
        break;
      case MM_MODEM_GSM_NETWORK_REG_STATUS_HOME:
      case MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING:
        mm_modem_state = MM_MODEM_STATE_REGISTERED;
        break;
      case MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING:
        mm_modem_state = MM_MODEM_STATE_SEARCHING;
        break;
      case MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN:
        mm_modem_state = MM_MODEM_STATE_ENABLED; // ???
        break;
      default:
        LOG(ERROR) << "Unknown registration status: " << registration_status;
        mm_modem_state = MM_MODEM_STATE_ENABLED; // ???
        break;
    }
    if (mm_modem_state != MM_MODEM_STATE_REGISTERED ||
        mm_state() <= MM_MODEM_STATE_SEARCHING)
      SetMmState(mm_modem_state, MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
  }
}

#define MASKVAL(cap) (1 << static_cast<int>(cap))
#define HASCAP(mask, cap) (mask & MASKVAL(cap))

static uint32_t DataCapabilitiesToMmAccessTechnology(BYTE num_data_caps,
                                                     ULONG* data_caps) {
  uint32_t capmask = 0;
  if (num_data_caps == 0)  // TODO(ers) indicates not registered?
    return MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
  // Put the values into a bit mask, where they'll be easier
  // to work with.
  for (int i = 0; i < num_data_caps; i++) {
    LOG(INFO) << "  Cap: " << static_cast<int>(data_caps[i]);
    if (data_caps[i] >= gobi::kDataCapGprs &&
        data_caps[i] <= gobi::kDataCapGsm)
      capmask |= 1 << static_cast<int>(data_caps[i]);
  }
  // Of the data capabilities reported, select the one with the
  // highest theoretical bandwidth.
  uint32_t mm_access_tech;
  switch (capmask & (MASKVAL(gobi::kDataCapHsdpa) |
                     (MASKVAL(gobi::kDataCapHsupa)))) {
    case MASKVAL(gobi::kDataCapHsdpa) | MASKVAL(gobi::kDataCapHsupa):
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_HSPA;
      break;
    case MASKVAL(gobi::kDataCapHsupa):
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_HSUPA;
      break;
    case MASKVAL(gobi::kDataCapHsdpa):
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_HSDPA;
      break;
    default:
      if (HASCAP(capmask, gobi::kDataCapWcdma))
        mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_UMTS;
      else if (HASCAP(capmask, gobi::kDataCapEdge))
        mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_EDGE;
      else if (HASCAP(capmask, gobi::kDataCapGprs))
        mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_GPRS;
      else if (HASCAP(capmask, gobi::kDataCapGsm))
        mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_GSM;
      else
        mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
      break;
  }
  LOG(INFO) << "MM access tech: " << mm_access_tech;
  return mm_access_tech;
}

#undef MASKVAL
#undef HASCAP

void GobiGsmModem::DataCapabilitiesHandler(BYTE num_data_caps,
                                           ULONG* data_caps) {
  LOG(INFO) << "GsmDataCapabilitiesHandler";
  uint32_t registration_status;
  std::string operator_code;
  std::string operator_name;
  DBus::Error error;

  // Sometimes when we lose registration, we don't get a
  // RegistrationStateChange callback, but we often do get
  // a DataCapabilitiesHandler callback!
  GetGsmRegistrationInfo(&registration_status,
                         &operator_code, &operator_name, error);
  switch(registration_status) {
    case MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE:
    case MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING:
    case MM_MODEM_GSM_NETWORK_REG_STATUS_DENIED:
      RegistrationInfo(registration_status, operator_code, operator_name);
      SetMmState(MM_MODEM_STATE_ENABLED, MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
      break;
    case MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN:
      // Ignore the unknown state.  The registration state will
      // eventually be reported as IDLE, SEACHING, DENIED, etc...
      break;
    case MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING:
    case MM_MODEM_GSM_NETWORK_REG_STATUS_HOME:
      SendNetworkTechnologySignal(
          DataCapabilitiesToMmAccessTechnology(num_data_caps, data_caps));
      break;
  }
}

void GobiGsmModem::DataBearerTechnologyHandler(ULONG technology) {
  uint32_t mm_access_tech;
  LOG(INFO) << "DataBearerTechnologyHandler: " << technology;
  switch (technology) {
    case gobi::kDataBearerGprs:
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_GPRS;
      break;
    case gobi::kDataBearerWcdma:
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_UMTS;
      break;
    case gobi::kDataBearerEdge:
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_EDGE;
      break;
    case gobi::kDataBearerHsdpaDlWcdmaUl:
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_HSDPA;
      break;
    case gobi::kDataBearerWcdmaDlUsupaUl:
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_HSUPA;
      break;
    case gobi::kDataBearerHsdpaDlHsupaUl:
      mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_HSPA;
      break;
    default:
        mm_access_tech = MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
        break;
  }
  SendNetworkTechnologySignal(mm_access_tech);
}

void GobiGsmModem::SendNetworkTechnologySignal(uint32_t mm_access_tech) {
  if (mm_access_tech != MM_MODEM_GSM_ACCESS_TECH_UNKNOWN) {
    AccessTechnology = mm_access_tech;
    utilities::DBusPropertyMap props;
    props["AccessTechnology"].writer().append_uint32(mm_access_tech);
    MmPropertiesChanged(
        org::freedesktop::ModemManager::Modem::Gsm::Network_adaptor
        ::introspect()->name, props);
  }
}

gboolean GobiGsmModem::CheckDataCapabilities(gpointer data) {
  CallbackArgs* args = static_cast<CallbackArgs*>(data);
  GobiGsmModem* modem =
      static_cast<GobiGsmModem *>(handler_->LookupByDbusPath(*args->path));
  delete args;
  if (modem != NULL)
    modem->SendNetworkTechnologySignal(modem->GetMmAccessTechnology());
  return FALSE;
}

gboolean GobiGsmModem::NewSmsCallback(gpointer data) {
  NewSmsArgs* args = static_cast<NewSmsArgs*>(data);
  LOG(INFO) << "New SMS Callback: type " << args->storage_type
            << " index " << args->message_index;
  GobiGsmModem* modem =
      static_cast<GobiGsmModem *>(handler_->LookupByDbusPath(*args->path));
  if (modem == NULL)
    return FALSE;

  DBus::Error error;
  SmsMessage* sms = modem->sms_cache_.SmsReceived(args->message_index, error,
                                                  modem);
  if (error.is_set())
    return FALSE;

  modem->SmsReceived(sms->index(), sms->IsComplete());

  return FALSE;
}

void GobiGsmModem::RegisterCallbacks() {
  GobiModem::RegisterCallbacks();
  sdk_->SetNewSMSCallback(NewSmsCallbackTrampoline);
}

static std::string MakeOperatorCode(WORD mcc, WORD mnc) {
  std::ostringstream opercode;
  if (mcc != 0xffff && mnc != 0xffff)
    opercode << mcc << mnc;
  return opercode.str();
}

// Trims any whitespace from both ends of the input string
// Local implementation to avoid the need to pull in <base/string_util.h>
static void TrimWhitespaceASCII(const std::string &input, std::string *output)
{
  size_t start, end, size;

  size = input.size();

  for (start = 0 ; start < size && isspace(input[start]); start++)
    ;
  for (end = size; end > start && isspace(input[end-1]); end--)
    ;

  *output = input.substr(start, end-start);
}


// returns <registration status, operator code, operator name>
void GobiGsmModem::GetGsmRegistrationInfo(uint32_t* registration_state,
                                          std::string* operator_code,
                                          std::string* operator_name,
                                          DBus::Error& error) {
  ULONG gobi_reg_state, roaming_state;
  ULONG l1;
  WORD mcc, mnc;
  CHAR netname[32];
  BYTE radio_interfaces[10];
  BYTE num_radio_interfaces = sizeof(radio_interfaces)/sizeof(BYTE);

  ULONG rc = sdk_->GetServingNetwork(&gobi_reg_state, &l1,
                                     &num_radio_interfaces,
                                     radio_interfaces, &roaming_state,
                                     &mcc, &mnc, sizeof(netname), netname);
  if (rc != 0) {
    // All errors are treated as if the registration state is unknown
    gobi_reg_state = gobi::kRegistrationStateUnknown;
    mcc = 0xffff;
    mnc = 0xffff;
    netname[0] = '\0';
  }

  switch (gobi_reg_state) {
    case gobi::kUnregistered:
      *registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE;
      break;
    case gobi::kRegistered:
      // TODO(ers) should RoamingPartner be reported as HOME?
      if (roaming_state == gobi::kHome)
        *registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_HOME;
      else
        *registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
      break;
    case gobi::kSearching:
      *registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING;
      break;
    case gobi::kRegistrationDenied:
      *registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_DENIED;
      break;
    case gobi::kRegistrationStateUnknown:
      *registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN;
      break;
  }
  *operator_code = MakeOperatorCode(mcc, mnc);
  TrimWhitespaceASCII(netname, operator_name);
  LOG(INFO) << "GSM reg info: "
            << *registration_state << ", "
            << *operator_code << ", "
            << *operator_name;
}

// Determine the current network technology and map it to
// ModemManager's MM_MODEM_GSM_ACCESS_TECH enum
uint32_t GobiGsmModem::GetMmAccessTechnology() {
  BYTE data_caps[48];
  BYTE num_data_caps = 12;
  DBus::Error error;

  ULONG rc = sdk_->GetServingNetworkCapabilities(&num_data_caps, data_caps);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetServingNetworkCapabilities, rc, kSdkError,
                                 MM_MODEM_GSM_ACCESS_TECH_UNKNOWN);

  return DataCapabilitiesToMmAccessTechnology(
      num_data_caps,
      reinterpret_cast<ULONG*>(data_caps));
}

bool GobiGsmModem::GetPinStatus(bool* enabled,
                                std::string* status,
                                uint32_t* retries_left) {
  ULONG pin_status, verify_retries_left, unblock_retries_left;
  DBus::Error error;

  ULONG rc = sdk_->UIMGetPINStatus(gobi::kPinId1, &pin_status,
                                   &verify_retries_left,
                                   &unblock_retries_left);
  ENSURE_SDK_SUCCESS_WITH_RESULT(UIMGetPINStatus, rc, kPinError, false);
  if (pin_status == gobi::kPinStatusPermanentlyBlocked) {
      *status = "sim-puk";
      *retries_left = 0;
  } else if (pin_status == gobi::kPinStatusBlocked) {
      *status = "sim-puk";
      *retries_left = unblock_retries_left;
  } else if (pin_status == gobi::kPinStatusNotInitialized ||
             pin_status == gobi::kPinStatusVerified ||
             pin_status == gobi::kPinStatusDisabled) {
      *status = "";
      if (verify_retries_left != gobi::kPinRetriesLeftUnknown)
        *retries_left = verify_retries_left;
      else
        *retries_left = kPinRetriesNotKnown;
  } else if (pin_status == gobi::kPinStatusEnabled) {
      *status = "sim-pin";
      *retries_left = verify_retries_left;
  }
  *enabled = pin_status != gobi::kPinStatusDisabled &&
             pin_status != gobi::kPinStatusNotInitialized;
  return true;
}


bool GobiGsmModem::CheckEnableOk(DBus::Error &error) {
  ULONG pin_status, verify_retries_left, unblock_retries_left;
  ULONG error_code;
  ULONG rc = sdk_->UIMGetPINStatus(gobi::kPinId1, &pin_status,
                                   &verify_retries_left,
                                   &unblock_retries_left);
  ENSURE_SDK_SUCCESS_WITH_RESULT(UIMGetPINStatus, rc, kPinError, false);
  const char *errname;
  switch (pin_status) {
    case gobi::kPinStatusNotInitialized:
    case gobi::kPinStatusVerified:
    case gobi::kPinStatusDisabled:
      return true;
    case gobi::kPinStatusEnabled:
      error_code = gobi::kAccessToRequiredEntityNotAvailable;
      break;
    case gobi::kPinStatusBlocked:
      error_code = gobi::kPinBlocked;
      break;
    case gobi::kPinStatusPermanentlyBlocked:
      error_code = gobi::kPinPermanentlyBlocked;
      break;
    default:
      error.set(kPinError, "UIMGetPINStatus: Unkown status");
      return false;
  }
  errname = QMIReturnCodeToMMError(error_code);
  if (errname == NULL)
    error.set(kPinError, "PIN error");
  else
    error.set(errname, "PIN locked");
  return false;
}

void GobiGsmModem::SetTechnologySpecificProperties() {
  AccessTechnology = GetMmAccessTechnology();
  bool enabled;
  std::string status;
  uint32_t retries_left;
  if (!GetPinStatus(&enabled, &status, &retries_left))
    retries_left = kPinRetriesNotKnown;
  UnlockRequired = status;
  UnlockRetries = retries_left;
  EnabledFacilityLocks = enabled ? MM_MODEM_GSM_FACILITY_SIM : 0;
  // TODO(ers) also need to set AllowedModes property. For the Gsm.Card
  // interface, need to set SupportedBands and SupportedModes properties
}

void GobiGsmModem::UpdatePinStatus() {
  bool enabled;
  std::string status;
  uint32_t retries_left;
  uint32_t mm_enabled_locks;

  if (!GetPinStatus(&enabled, &status, &retries_left))
    return;

  UnlockRequired = status;
  UnlockRetries = retries_left;
  mm_enabled_locks = enabled ? MM_MODEM_GSM_FACILITY_SIM : 0;
  EnabledFacilityLocks = mm_enabled_locks;

  utilities::DBusPropertyMap props;
  props["UnlockRequired"].writer().append_string(status.c_str());
  props["UnlockRetries"].writer().append_uint32(retries_left);
  MmPropertiesChanged(
      org::freedesktop::ModemManager::Modem_adaptor::introspect()->name, props);
  utilities::DBusPropertyMap cardprops;
  cardprops["EnabledFacilityLocks"].writer().append_uint32(mm_enabled_locks);
  MmPropertiesChanged(
      org::freedesktop::ModemManager::Modem::Gsm::Card_adaptor
                                                          ::introspect()->name,
      cardprops);
}

void GobiGsmModem::GetTechnologySpecificStatus(
    utilities::DBusPropertyMap* properties) {
}

//======================================================================
// DBUS Methods: Modem.Gsm.Network

void GobiGsmModem::Register(const std::string& network_id,
                            DBus::Error& error) {
  ULONG rc;
  ULONG regtype, rat;
  WORD mcc, mnc;
  // This is a blocking call, and may take a while (up to 30 seconds)

  LOG(INFO) << "Register request for [" << network_id << "]";
  if (network_id.empty()) {
    regtype = gobi::kRegistrationTypeAutomatic;
    mcc = 0;
    mnc = 0;
    rat = 0;
    LOG(INFO) << "Initiating automatic registration";
  } else {
    int n = sscanf(network_id.c_str(), "%3hu%3hu", &mcc, &mnc);
    if (n != 2) {
      error.set(kRegistrationError, "Malformed network ID");
      return;
    }
    regtype = gobi::kRegistrationTypeManual;
    rat = gobi::kRfiUmts;
    LOG(INFO) << "Initiating manual registration for " << mcc << mnc;
  }
  rc = sdk_->InitiateNetworkRegistration(regtype, mcc, mnc, rat);
  if (rc == gobi::kOperationHasNoEffect)
    return;  // already registered on requested network
  ENSURE_SDK_SUCCESS(InitiateNetworkRegistration, rc, kSdkError);
}

ScannedNetworkList GobiGsmModem::Scan(DBus::Error& error) {
  gobi::GsmNetworkInfoInstance networks[40];
  BYTE num_networks = sizeof(networks)/sizeof(networks[0]);
  ScannedNetworkList list;

  // This is a blocking call, and may take a while (i.e., a minute or more)
  LOG(INFO) << "Beginning network scan";
  ULONG rc = sdk_->PerformNetworkScan(&num_networks,
                                    static_cast<BYTE*>((void *)&networks[0]));
  ENSURE_SDK_SUCCESS_WITH_RESULT(PerformNetworkScan, rc, kSdkError, list);
  LOG(INFO) << "Network scan returned " << (int)num_networks << " networks";
  for (int i = 0; i < num_networks; i++) {
    gobi::GsmNetworkInfoInstance *net = &networks[i];
    std::map<std::string, std::string> netprops;
    // status, operator-long, operator-short, operator-num, access-tech
    const char* status;
    if (net->inUse == gobi::kGsmNetInfoYes)
      status = "2";
    else if (net->forbidden == gobi::kGsmNetInfoYes)
      status = "3";
    else if (net->inUse == gobi::kGsmNetInfoNo)
      status = "1";
    else
      status = "0";
    netprops["status"] = status;
    netprops["operator-num"] = MakeOperatorCode(net->mcc, net->mnc);
    if (strlen(net->description) != 0) {
      std::string operator_name;
      TrimWhitespaceASCII(net->description, &operator_name);
      netprops["operator-short"] = operator_name;
    }
    list.push_back(netprops);
  }
  return list;
}

void GobiGsmModem::SetApn(const std::string& apn, DBus::Error& error) {
  LOG(WARNING) << "GobiGsmModem::SetApn not implemented";
}

uint32_t GobiGsmModem::GetSignalQuality(DBus::Error& error) {
  return GobiModem::CommonGetSignalQuality(error);
}

void GobiGsmModem::SetBand(const uint32_t& band, DBus::Error& error) {
  LOG(WARNING) << "GobiGsmModem::SetBand not implemented";
}

uint32_t GobiGsmModem::GetBand(DBus::Error& error) {
  LOG(WARNING) << "GobiGsmModem::GetBand not implemented";
  return 0;
}

void GobiGsmModem::SetNetworkMode(const uint32_t& mode, DBus::Error& error) {
  LOG(WARNING) << "GobiGsmModem::SetNetworkMode not implemented";
}

uint32_t GobiGsmModem::GetNetworkMode(DBus::Error& error) {
  LOG(WARNING) << "GobiGsmModem::GetNetworkMode not implemented";
  return 0;
}

// returns <registration status, operator code, operator name>
// reg status = idle, home, searching, denied, unknown, roaming
DBus::Struct<uint32_t,
    std::string,
    std::string> GobiGsmModem::GetRegistrationInfo(
    DBus::Error& error) {
  DBus::Struct<uint32_t, std::string, std::string> result;
  GetGsmRegistrationInfo(&result._1, &result._2, &result._3, error);
  // We don't always get an SDK callback when the network technology
  // changes, so simulate a callback here to make sure that the
  // most up-to-date idea of network technology gets signaled.
  PostCallbackRequest(CheckDataCapabilities, new CallbackArgs());
  return result;
}

void GobiGsmModem::SetAllowedMode(const uint32_t& mode,
                                  DBus::Error& error) {
  LOG(WARNING) << "GobiGsmModem::SetAllowedMmode not implemented";
}

//======================================================================
// DBUS Methods: Modem.Gsm.Card

std::string GobiGsmModem::GetImei(DBus::Error& error) {
  SerialNumbers serials;
  ScopedApiConnection connection(*this);
  connection.ApiConnect(error);
  if (error.is_set())
    return "";
  GetSerialNumbers(&serials, error);
  return error.is_set() ? "" : serials.imei;
}

std::string GobiGsmModem::GetImsi(DBus::Error& error) {
  char imsi[kDefaultBufferSize];
  ScopedApiConnection connection(*this);
  connection.ApiConnect(error);
  if (error.is_set())
    return "";
  ULONG rc = sdk_->GetIMSI(kDefaultBufferSize, imsi);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetIMSI, rc, kSdkError, "");
  return imsi;
}

void GobiGsmModem::SendPuk(const std::string& puk,
                           const std::string& pin,
                           DBus::Error& error) {
  ULONG verify_retries_left;
  ULONG unblock_retries_left;
  CHAR *pukP = const_cast<CHAR*>(puk.c_str());
  CHAR *pinP = const_cast<CHAR*>(pin.c_str());

  // If we're not enabled, then we're not connected to the SDK
  ScopedApiConnection connection(*this);
  DBus::Error tmperror;
  connection.ApiConnect(tmperror);
  ULONG rc = sdk_->UIMUnblockPIN(gobi::kPinId1, pukP, pinP,
                                 &verify_retries_left,
                                 &unblock_retries_left);
  UpdatePinStatus();
  ENSURE_SDK_SUCCESS(UIMUnblockPIN, rc, kPinError);
}

void GobiGsmModem::SendPin(const std::string& pin, DBus::Error& error) {
  ULONG verify_retries_left;
  ULONG unblock_retries_left;
  CHAR* pinP = const_cast<CHAR*>(pin.c_str());

  // If we're not enabled, then we're not connected to the SDK
  ScopedApiConnection connection(*this);
  DBus::Error tmperror;
  connection.ApiConnect(tmperror);

  ULONG rc = sdk_->UIMVerifyPIN(gobi::kPinId1, pinP, &verify_retries_left,
                                &unblock_retries_left);
  UpdatePinStatus();
  ENSURE_SDK_SUCCESS(UIMVerifyPIN, rc, kPinError);
}

void GobiGsmModem::EnablePin(const std::string& pin,
                             const bool& enabled,
                             DBus::Error& error) {
  ULONG bEnable = enabled;
  ULONG verify_retries_left;
  ULONG unblock_retries_left;
  CHAR* pinP = const_cast<CHAR*>(pin.c_str());

  ULONG rc = sdk_->UIMSetPINProtection(gobi::kPinId1, bEnable,
                                       pinP, &verify_retries_left,
                                       &unblock_retries_left);
  UpdatePinStatus();
  if (rc == gobi::kOperationHasNoEffect)
    return;
  ENSURE_SDK_SUCCESS(UIMSetPINProtection, rc, kPinError);
}

void GobiGsmModem::ChangePin(const std::string& old_pin,
                             const std::string& new_pin,
                             DBus::Error& error) {
  ULONG verify_retries_left;
  ULONG unblock_retries_left;
  CHAR* old_pinP = const_cast<CHAR*>(old_pin.c_str());
  CHAR* new_pinP = const_cast<CHAR*>(new_pin.c_str());

  ULONG rc = sdk_->UIMChangePIN(gobi::kPinId1, old_pinP, new_pinP,
                                &verify_retries_left, &unblock_retries_left);
  UpdatePinStatus();
  ENSURE_SDK_SUCCESS(UIMChangePIN, rc, kPinError);
}

std::string GobiGsmModem::GetOperatorId(DBus::Error& error) {
  std::string result;
  WORD mcc, mnc, sid, nid;
  CHAR netname[32];

  ULONG rc = sdk_->GetHomeNetwork(&mcc, &mnc,
                                  sizeof(netname), netname, &sid, &nid);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetHomeNetwork, rc, kSdkError, result);
  return MakeOperatorCode(mcc, mnc);
}

std::string GobiGsmModem::GetSpn(DBus::Error& error) {
  std::string result;

  BYTE names[256];
  ULONG namesLen = sizeof(names);

  ULONG rc = sdk_->GetPLMNName(0, 0, &namesLen, names);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetPLMNName, rc, kSdkError, result);

  if (namesLen < 2) {
    LOG(WARNING) << "GetSpn: name structure too short"
                 << " (" << namesLen << " < 2)";
    return "";
  }

  size_t spnLen = names[1];

  if (namesLen - 2 < (ULONG)spnLen) {
    LOG(WARNING) << "GetSpn: SPN length too long"
                 << " (" << (ULONG)spnLen << " < " << namesLen << " - 2)";
    return "";
  }

  switch (names[0]) {
  case 0x00: // ASCII
    {
      const char *spnAscii = (const char *)names + 2;
      result = std::string(spnAscii, spnLen);
    }
    break;
  case 0x01: // UCS2
    {
      const uint8_t *spnUcs2 = (const uint8_t *)names + 2;
      result = std::string(utilities::Ucs2ToUtf8String(spnUcs2, spnLen));
    }
    break;
  default:
    LOG(WARNING) << "GetSpn: invalid encoding byte " << names[0];
    break;
  }

  LOG(INFO) << " GetSpn: returning \"" << result << "\"";

  return result;
}

std::string GobiGsmModem::GetMsIsdn(DBus::Error& error) {
  char mdn[kDefaultBufferSize], min[kDefaultBufferSize];
  ULONG rc = sdk_->GetVoiceNumber(kDefaultBufferSize, mdn,
                            kDefaultBufferSize, min);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetVoiceNumber, rc, kSdkError, "");
  return mdn;
}

//======================================================================
// DBUS Methods: Modem.Gsm.SMS

SmsMessageFragment* GobiGsmModem::GetSms(int index, DBus::Error& error)
{
  ULONG tag, format, size;
  BYTE message[200];

  size = sizeof(message);
  ULONG rc = sdk_->GetSMS(gobi::kSmsNonVolatileMemory, index,
                          &tag, &format, &size, message);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetSMS, rc, kSdkError, NULL);

  LOG(INFO) << "GetSms: " << "tag " << tag << " format " << format
            << " size " << size;

  SmsMessageFragment *fragment =
      SmsMessageFragment::CreateFragment(message, size, index);
  if (fragment == NULL) {
    error.set(kInvalidArgumentError, "Couldn't decode PDU");
    LOG(WARNING) << "Couldn't decode PDU";
  }
  return fragment;
}

void GobiGsmModem::DeleteSms(int index, DBus::Error& error)
{
  ULONG lindex = index;
  ULONG rc = sdk_->DeleteSMS(gobi::kSmsNonVolatileMemory, &lindex, NULL);
  ENSURE_SDK_SUCCESS(DeleteSMS, rc, kSdkError);
}

std::vector<int>* GobiGsmModem::ListSms(DBus::Error& error)
{
  ULONG items[100];
  ULONG num_items;

  num_items = sizeof(items) / (2 * sizeof(items[0]));
  ULONG rc = sdk_->GetSMSList(gobi::kSmsNonVolatileMemory, NULL,
                              &num_items, (BYTE *)items);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetSMSList, rc, kSdkError, NULL);
  LOG(INFO) << "GetSmsList: got " << num_items << " messages";

  std::vector<int>* result = new std::vector<int>();
  for (ULONG i = 0 ; i < num_items ; i++)
    result->push_back(items[2 * i]);

  return result;
}

void GobiGsmModem::Delete(const uint32_t& index, DBus::Error &error) {
  sms_cache_.Delete(index, error, this);
}

utilities::DBusPropertyMap GobiGsmModem::Get(const uint32_t& index,
                                             DBus::Error &error) {
  scoped_ptr<utilities::DBusPropertyMap> message(
      sms_cache_.Get(index, error, this));
  return *(message.get());
}

std::vector<utilities::DBusPropertyMap> GobiGsmModem::List(DBus::Error &error) {
  scoped_ptr<std::vector<utilities::DBusPropertyMap> > list(
      sms_cache_.List(error, this));
  return *(list.get());
}

std::string GobiGsmModem::GetSmsc(DBus::Error &error) {
  CHAR address[100];
  CHAR address_type[100];

  std::string result;
  ULONG rc = sdk_->GetSMSCAddress(sizeof(address), address,
                                  sizeof(address_type), address_type);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetSMSCAddress, rc, kSdkError, result);
  LOG(INFO) << "SMSC address: " << address << " type: " << address_type;
  result = address;
  return result;
}

void GobiGsmModem::SetSmsc(const std::string& smsc, DBus::Error &error) {
  CHAR *addr = const_cast<CHAR*>(smsc.c_str());
  ULONG rc = sdk_->SetSMSCAddress(addr, NULL);
  ENSURE_SDK_SUCCESS(GetSMSCAddress, rc, kSdkError);
}


std::vector<uint32_t> GobiGsmModem::Save(
    const utilities::DBusPropertyMap& properties,
    DBus::Error &error) {
  LOG(WARNING) << "GobiGsmModem::Save not implemented";
  return std::vector<uint32_t>();
}

std::vector<uint32_t> GobiGsmModem::Send(
    const utilities::DBusPropertyMap& properties,
    DBus::Error &error) {
  LOG(WARNING) << "GobiGsmModem::Send not implemented";
  return std::vector<uint32_t>();
}

void GobiGsmModem::SendFromStorage(const uint32_t& index, DBus::Error &error) {
  LOG(WARNING) << "GobiGsmModem::SendFromStorage not implemented";
}

// What is this supposed to do?
void GobiGsmModem::SetIndication(const uint32_t& mode,
                                 const uint32_t& mt,
                                 const uint32_t& bm,
                                 const uint32_t& ds,
                                 const uint32_t& bfr,
                                 DBus::Error &error) {
  LOG(WARNING) << "GobiGsmModem::SetIndication not implemented";
}

// The API documentation says nothing about what this is supposed
// to return. Most likely it's intended to report whether messages
// are being sent and received in text mode or PDU mode. But the
// meanings of the return values are undocumented.
uint32_t GobiGsmModem::GetFormat(DBus::Error &error) {
  LOG(WARNING) << "GobiGsmModem::GetFormat not implemented";
  return 0;
}

// The API documentation says nothing about what this is supposed
// to return. Most likely it's intended for specifying whether messages
// are being sent and received in text mode or PDU mode. But the
// meanings of the argument values are undocumented.
void GobiGsmModem::SetFormat(const uint32_t& format, DBus::Error &error) {
  LOG(WARNING) << "GobiGsmModem::SetFormat not implemented";
}
