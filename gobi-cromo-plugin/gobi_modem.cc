// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem.h"

#include <algorithm>

#include <base/scoped_ptr.h>
#include <base/time.h>
#include <glog/logging.h>
#include <mm/mm-modem.h>

static const size_t kDefaultBufferSize = 128;
// TODO(ers) define constants for each distinct error we return
static const char* kErrorName = "org.freedesktop.ModemManager";

// from mm-modem.h in ModemManager. This enum
// should move into an XML file to become part
// of the DBus API
typedef enum {
    MM_MODEM_STATE_UNKNOWN = 0,
    MM_MODEM_STATE_DISABLED = 10,
    MM_MODEM_STATE_DISABLING = 20,
    MM_MODEM_STATE_ENABLING = 30,
    MM_MODEM_STATE_ENABLED = 40,
    MM_MODEM_STATE_SEARCHING = 50,
    MM_MODEM_STATE_REGISTERED = 60,
    MM_MODEM_STATE_DISCONNECTING = 70,
    MM_MODEM_STATE_CONNECTING = 80,
    MM_MODEM_STATE_CONNECTED = 90,

    MM_MODEM_STATE_LAST = MM_MODEM_STATE_CONNECTED
} MMModemState;

// The following constants define the granularity with which signal
// strength is reported, i.e., the number of bars.
//
// The values given here are used to compute an array of thresholds
// consisting of the values [-113, -98, -83, -68, -53], which results
// in the following mapping of signal strength as reported in dBm to
// bars displayed:
//
// <  -113             0 bars
// >= -113 and < -98   1 bar
// >=  -98 and < -83   2 bars
// >=  -83 and < -68   3 bars
// >=  -68 and < -53   4 bars
// >=  -53             5 bars

// Any reported signal strength larger than or equal to kMaxSignalStrengthDbm
// is considered full strength.
static const int kMaxSignalStrengthDbm = -53;
// Any reported signal strength smaller than kMaxSignalStrengthDbm is
// considered zero strength.
static const int kMinSignalStrengthDbm = -113;
// The number of signal strength levels we want to report, ranging from
// 0 bars to kSignalStrengthNumLevels-1 bars.
static const int kSignalStrengthNumLevels = 6;

static unsigned long signal_strength_dbm_to_percent(INT8 signal_strength_dbm) {
  unsigned long signal_strength_percent;

  if (signal_strength_dbm < kMinSignalStrengthDbm)
    signal_strength_percent = 0;
  else if (signal_strength_dbm >= kMaxSignalStrengthDbm)
    signal_strength_percent = 100;
  else
    signal_strength_percent =
        ((signal_strength_dbm - kMinSignalStrengthDbm) * 100 /
         (kMaxSignalStrengthDbm - kMinSignalStrengthDbm));
  return signal_strength_percent;
}

ULONG get_rfi_technology(ULONG data_bearer_technology) {
  switch (data_bearer_technology) {
    default:
      return gobi::kRfiCdmaEvdo;
  }
}


GobiModem* GobiModem::connected_modem_(NULL);

GobiModem::GobiModem(DBus::Connection& connection,
                     const DBus::Path& path,
                     GobiModemHandler *handler,
                     const DEVICE_ELEMENT &device,
                     gobi::Sdk *sdk)
    : DBus::ObjectAdaptor(connection, path),
      handler_(handler),
      sdk_(sdk),
      last_seen_(-1),
      activation_state_(0),
      session_state_(gobi::kDisconnected),
      session_id_(0),
      data_bearer_technology_(0),
      roaming_state_(0),
      signal_strength_(-127) {
  memcpy(&device_, &device, sizeof(device_));

  pthread_mutex_init(&activation_mutex_, NULL);
  pthread_cond_init(&activation_cond_, NULL);

  // Initialize DBus object properties
  //Device = device_.deviceNode;
  // TODO(ers) Hard-wire device to be usb0. This needs
  // to be the name of the network interface, but
  // device_.deviceNode is "/dev/qcqmi0", which is
  // not useful to flimflam. We need to find out
  // whether there is an API that can report the
  // name of the network interface.
  Device = "usb0";
  Driver = "";
  Enabled = false;
  IpMethod = MM_MODEM_IP_METHOD_DHCP;
  MasterDevice = "TODO(rochberg)";
  // TODO(rochberg):  Gobi can be either
  Type = MM_MODEM_TYPE_CDMA;
  UnlockRequired = "";

  // Reviewer note:  These are just temporary calls
  DBus::Error error;
  Enable(true, error);
  LogGobiInformation();
}

// DBUS Methods: Modem
void GobiModem::Enable(const bool& enable, DBus::Error& error) {
  if (enable && !Enabled()) {
    if (!ApiConnect()) {
      // TODO(rochberg): ERROR
      CHECK(0);
    }

    ULONG firmware_id;
    ULONG technology;
    ULONG carrier;
    ULONG region;
    ULONG gps_capability;
    ULONG rc;
    rc = sdk_->GetFirmwareInfo(&firmware_id,
                               &technology,
                               &carrier,
                               &region,
                               &gps_capability);
    if (rc != 0) {
      LOG(WARNING) << "GetFirmwareInfo: " << rc;
      // TODO(rochberg):  ERROR
      return;
    }
    LOG(INFO) << "Current carrier is " << carrier;

    // TODO(rochberg):  Make this more general
    if (carrier != 1  && // Generic
        carrier < 101) {  // Defined carriers start at 101
      SetCarrier("Sprint", error);
    }
    Enabled = true;
  }
}

void GobiModem::Connect(const std::string& unused_number, DBus::Error& error) {
  if (!Enabled()) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "Connect on disabled modem";
    return;
  }

  // TODO(ers) check activation status and return error if not activated
  ULONG rc;
  rc = sdk_->GetActivationState(&activation_state_);
  LOG(INFO) << "Activation state: " << activation_state_;
  if (activation_state_ != gobi::kActivated) {
    // TODO(ers) ERROR
    LOG(WARNING) << "Connect failed because modem is not activated";
    return;
  }

  ULONG failure_reason;
  LOG(INFO) << "Starting data session: ";
  rc = sdk_->StartDataSession(NULL,  // technology
                              NULL,  // APN Name
                              NULL,  // Authentication
                              NULL,  // Username
                              NULL,  // Password
                              &session_id_,  // OUT: session ID
                              &failure_reason  // OUT: failure reason
                        );
  if (rc != 0) {
    LOG(WARNING) << "StartDataSession failed: " << rc;
    if (rc == gobi::kCallFailed)
      LOG(INFO) << "Failure Reason " <<  failure_reason;
  }
  LOG(INFO) << "Session ID " <<  session_id_;
  // session_state_ will be set in SessionStateCallback
}


void GobiModem::Disconnect(DBus::Error& error) {
  LOG(INFO) << "Disconnect";
  if (session_state_ != gobi::kConnected) {
    LOG(WARNING) << "Disconnect called when not connecting";
    return;
  }
  ULONG rc = sdk_->StopDataSession(session_id_);
  if (rc != 0) {
    LOG(WARNING) << "StopDataSession failed: " << rc;
    // TODO(ers) ERROR
  }
  // session_state_ will be set in SessionStateCallback
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> GobiModem::GetIP4Config(
    DBus::Error& error) {
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  LOG(INFO) << "GetIP4Config";

  return result;
}

void GobiModem::FactoryReset(const std::string& code, DBus::Error& error) {
  ULONG rc;
  LOG(INFO) << "ResetToFactoryDefaults";
  rc = sdk_->ResetToFactoryDefaults(const_cast<CHAR *>(code.c_str()));
  if (rc != 0) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "Factory reset failed: " << rc;
    return;
  }
  // TODO(rochberg): check ResetModem and ERROR
  ResetModem();

  return;
}

DBus::Struct<std::string, std::string, std::string> GobiModem::GetInfo(
    DBus::Error& error) {
  // (manufacturer, modem, version).
  DBus::Struct<std::string, std::string, std::string> result;

  char buffer[kDefaultBufferSize];
  ULONG rc;
  rc = sdk_->GetManufacturer(sizeof(buffer), buffer);
  if (rc != 0) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetManufacturer: " << rc;
    return result;
  }

  result._1 = buffer;

  rc = sdk_->GetModelID(sizeof(buffer), buffer);
  if (rc != 0) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetModelID: " << rc;
    return result;
  }
  result._2 = buffer;

  rc = sdk_->GetFirmwareRevision(sizeof(buffer), buffer);
  if (rc != 0) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetFirmwareRevision: " << rc;
    return result;
  }
  result._3 = buffer;

  LOG(INFO) << "Manufacturer: " << result._1;
  LOG(INFO) << "Modem: " << result._2;
  LOG(INFO) << "Version: " << result._3;
  return result;
}

// DBUS Methods: ModemSimple
void GobiModem::Connect(const DBusPropertyMap& properties, DBus::Error& error) {
  LOG(INFO) << "Simple.Connect";
  Connect("unused_number", error);
}

bool GobiModem::GetSerialNumbers(SerialNumbers *out) {
  char esn[kDefaultBufferSize];
  char imei[kDefaultBufferSize];
  char meid[kDefaultBufferSize];
  ULONG rc = sdk_-> GetSerialNumbers(kDefaultBufferSize, esn,
                                     kDefaultBufferSize, imei,
                                     kDefaultBufferSize, meid);
  if (rc != 0) {
    LOG(WARNING) << "GSN: " << rc;
    return false;
  }
  out->esn = esn;
  out->imei = imei;
  out->meid = meid;
  return true;
}

DBusPropertyMap GobiModem::GetStatus(DBus::Error& error) {
  DBusPropertyMap result;

  int32_t rssi;

  // TODO(rochberg):  More mandatory properties expected
  if (GetSignalStrengthDbm(rssi)) {
    result["signal_strength_dbm"].writer().append_int32(rssi);
  }

  SerialNumbers serials;

  if (this->GetSerialNumbers(&serials)) {
    if (serials.esn.length()) {
      result["esn"].writer().append_string(serials.esn.c_str());
    }
    if (serials.meid.length()) {
      result["meid"].writer().append_string(serials.meid.c_str());
    }
    if (serials.imei.length()) {
      result["imei"].writer().append_string(serials.imei.c_str());
    }
  }

  ULONG activation_state;
  ULONG rc = sdk_->GetActivationState(&activation_state);
  if (rc == 0)
    result["activation_state"].writer().append_uint32(activation_state);

  ULONG session_state;
  ULONG mm_modem_state;
  rc = sdk_->GetSessionState(&session_state);
  if (rc == 0) {
    // TODO(ers) if not registered, should return enabled state
    switch (session_state) {
      case gobi::kConnected:
        mm_modem_state = MM_MODEM_STATE_CONNECTED;
        break;
      case gobi::kAuthenticating:
        mm_modem_state = MM_MODEM_STATE_CONNECTING;
        break;
      case gobi::kDisconnected:
      default:
        ULONG reg_state;
        ULONG dc1;      // don't care
        BYTE dc3[10];
        BYTE dc2 = sizeof(dc3)/sizeof(BYTE);
        rc = sdk_->GetServingNetwork(&reg_state,
                                     &dc2,
                                     dc3,
                                     &dc1);
        if (rc == 0) {
          if (reg_state == gobi::kRegistered) {
            mm_modem_state = MM_MODEM_STATE_REGISTERED;
            break;
          } else if (reg_state == gobi::kSearching) {
            mm_modem_state = MM_MODEM_STATE_SEARCHING;
            break;
          }
        }
        mm_modem_state = MM_MODEM_STATE_UNKNOWN;
    }
    result["state"].writer().append_uint32(mm_modem_state);
  }

  return result;
}

  // DBUS Methods: ModemCDMA

std::string GobiModem::GetEsn(DBus::Error& error) {
  LOG(INFO) << "GetEsn";

  SerialNumbers serials;
  if (!GetSerialNumbers(&serials)) {
    // TODO(rochberg): ERROR
    CHECK(0);
  }
  return serials.esn;
}


uint32_t GobiModem::GetSignalQuality(DBus::Error& error) {
  if (!Enabled()) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetSignalQuality on disabled modem";
    return 0;
  }
  int32_t signal_strength_dbm;
  if (GetSignalStrengthDbm(signal_strength_dbm)) {
    uint32_t result = signal_strength_dbm_to_percent(signal_strength_dbm);
    LOG(INFO) << "GetSignalQuality => " << result << "%";
    return result;
  }
  // TODO(ers) ERROR
  return 11;
}


// returns <band class, band, system id>
DBus::Struct<uint32_t, std::string, uint32_t> GobiModem::GetServingSystem(
    DBus::Error& error) {
  DBus::Struct<uint32_t, std::string, uint32_t> result;
  ULONG rc;
  WORD mcc, mnc, sid, nid;
  CHAR netname[32];
  LOG(INFO) << "GetServingSystem";

  rc = sdk_->GetHomeNetwork(&mcc, &mnc, sizeof(netname), netname, &sid, &nid);
  if (rc != 0) {
    // TODO(ers) ERROR
    LOG(WARNING) << "GetHomeNetwork failed: " << rc;
    return result;
  }
  gobi::RfInfoInstance rf_info[10];
  BYTE rf_info_size = sizeof(rf_info) / sizeof(rf_info[0]);
  rc = sdk_->GetRFInfo(&rf_info_size, static_cast<BYTE*>((void *)&rf_info[0]));
  if (rc != 0) {
    // TODO(ers) ERROR
    LOG(WARNING) << "GetRFInfo failed: " << rc;
    return result;
  }
  if (rf_info_size != 0) {
    LOG(INFO) << "RF info for " << rf_info[0].radioInterface
              << " band class " << rf_info[0].activeBandClass
              << " channel "    << rf_info[0].activeChannel;
    switch (rf_info[0].activeBandClass) {
      case 0:
      case 85:          // WCDMA 800
        result._1 = 1;  // 800 Mhz band class
        break;
      case 1:
      case 81:          // WCDMA PCS 1900
        result._1 = 2;  // 1900 Mhz band class
        break;
      default:
        result._1 = 0;  // unknown band class
        break;
    }
    result._2 = "F";    // XXX bogus
  }
  result._3 = sid;

  return result;
}

void GobiModem::GetRegistrationState(uint32_t& cdma_1x_state,
                                     uint32_t& evdo_state,
                                     DBus::Error &error) {
  LOG(INFO) << "GetRegistrationState";
  ULONG rc;
  ULONG reg_state;
  ULONG roaming_state;
  BYTE radio_interfaces[10];
  BYTE num_radio_interfaces = sizeof(radio_interfaces)/sizeof(BYTE);

  cdma_1x_state = 0;
  evdo_state = 0;
  rc = sdk_->GetServingNetwork(&reg_state,
                               &num_radio_interfaces,
                               radio_interfaces,
                               &roaming_state);
  if (rc != 0) {
    // TODO(ers) Error
    LOG(WARNING) << "GetServingNetwork() failed: " << rc;
    return;
  }
  uint32_t mm_reg_state;

  if (reg_state == gobi::kRegistered) {
    if (roaming_state == gobi::kHome)
      mm_reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
    else
      mm_reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
  } else {
    mm_reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  }

  for (int i = 0; i < num_radio_interfaces; i++) {
    LOG(INFO) << "Registration state " << reg_state
              << " for RFI " << (int)radio_interfaces[i];
    if (radio_interfaces[i] == gobi::kRfiCdma1xRtt)
      cdma_1x_state = mm_reg_state;
    else if (radio_interfaces[i] == gobi::kRfiCdmaEvdo)
      evdo_state = mm_reg_state;
  }
}

bool GobiModem::ApiConnect() {
  ULONG rc;
  rc = sdk_->QCWWANConnect(device_.deviceNode, device_.deviceKey);
  if (rc != 0) {
    return false;
  }
  connected_modem_ = this;

  SetActivationStatusCallback(ActivationStatusCallbackTrampoline);
  SetNMEAPlusCallback(NmeaPlusCallbackTrampoline);
  SetOMADMStateCallback(OmadmStateCallbackTrampoline);
  SetSessionStateCallback(SessionStateCallbackTrampoline);
  SetDataBearerCallback(DataBearerCallbackTrampoline);
  SetRoamingIndicatorCallback(RoamingIndicatorCallbackTrampoline);

  int num_thresholds = kSignalStrengthNumLevels - 1;
  int interval =
      (kMaxSignalStrengthDbm - kMinSignalStrengthDbm) /
      (kSignalStrengthNumLevels - 1);
  scoped_array<INT8> thresholds(new INT8[num_thresholds]);
  for (int i = 0; i < num_thresholds; i++) {
    thresholds[i] = kMinSignalStrengthDbm + interval * i;
  }

  SetSignalStrengthCallback(SignalStrengthCallbackTrampoline,
                            num_thresholds,
                            thresholds.get());

  return true;
}

void GobiModem::LogGobiInformation() {
  ULONG rc;

  char buffer[kDefaultBufferSize];
  rc = sdk_->GetManufacturer(sizeof(buffer), buffer);
  LOG(INFO) << "Manufacturer: " << buffer;

  ULONG firmware_id;
  ULONG technology;
  ULONG carrier;
  ULONG region;
  ULONG gps_capability;
  rc = sdk_->GetFirmwareInfo(&firmware_id,
                             &technology,
                             &carrier,
                             &region,
                             &gps_capability);
  CHECK(rc == 0) << rc ;
  LOG(INFO) << "Firmware info: "
            << "firmware_id: " << firmware_id
            << " technology: " << technology
            << " carrier: " << carrier
            << " region: " << region
            << " gps_capability: " << gps_capability;

  char amss[kDefaultBufferSize], boot[kDefaultBufferSize];
  char pri[kDefaultBufferSize];
  rc = sdk_->GetFirmwareRevisions(kDefaultBufferSize, amss,
                                  kDefaultBufferSize, boot,
                                  kDefaultBufferSize, pri);
  if (rc == 0) {
    LOG(INFO) << "Firmware Revisions: AMSS: " << amss
              << " boot: " << boot
              << " pri: " << pri;
  } else {
    LOG(WARNING) << "GFR: " << rc;
  }

  rc = GetImageStore(sizeof(buffer), buffer);
  if (rc == 0) {
    LOG(INFO) << "ImageStore: " << buffer;
  } else {
    LOG(WARNING) << "GIS: " << rc;
  }

  SerialNumbers serials;
  CHECK(GetSerialNumbers(&serials));
  LOG(INFO) << "ESN: " << serials.esn;
  LOG(INFO) << "IMEI: " << serials.imei;
  LOG(INFO) << "MEID: " << serials.meid;

  char number[kDefaultBufferSize], min_array[kDefaultBufferSize];
  rc = GetVoiceNumber(kDefaultBufferSize, number,
                      kDefaultBufferSize, min_array);
  if (rc == 0) {
    LOG(INFO) << "Voice: " << number
              << " MIN: " << min_array;
  } else if (rc != gobi::kNotProvisioned) {
    LOG(INFO) << "GetVoiceNumber failed: " << rc;
  }

  BYTE index;
  rc = GetActiveMobileIPProfile(&index);
  if (rc != 0 && rc != gobi::kNotSupportedByDevice) {
    LOG(WARNING) << "GetAMIPP: " << rc;
  } else {
    LOG(INFO) << "Mobile IP profile: " << (int)index;
  }
}

void GobiModem::SoftReset(DBus::Error& error) {
  if (!ResetModem()) {
    // TODO(rochberg):  ERROR
  }
}

bool GobiModem::ResetModem() {
  ULONG rc;

  // TODO(rochberg):  Is this going to confuse connman?
  Enabled = false;

  LOG(INFO) << "Offline";
  rc = sdk_->SetPower(gobi::kOffline);
  if (rc != 0) {
    LOG(WARNING) << "Offline: " << rc;
    // TODO(rochberg):  Disconnect?
    return false;
  }

  LOG(INFO) << "Reset";
  rc = sdk_->SetPower(gobi::kReset);
  if (rc != 0) {
    LOG(WARNING) << "Offline: " << rc;
    return false;
  }

  const int kPollIntervalMs = 500;
  base::Time::Time deadline = base::Time::NowFromSystemTime() +
      base::TimeDelta::FromSeconds(60);

  bool connected = false;
  // Wait for modem to become unavailable, then available again.
  while (base::Time::NowFromSystemTime() < deadline) {
    sdk_->QCWWANDisconnect();
    if (!ApiConnect()) {
      break;
    }
    usleep(kPollIntervalMs * 1000);
  }

  LOG(INFO) << "Modem reset:  Modem now unavailable";

  while (base::Time::NowFromSystemTime() < deadline) {
    sdk_->QCWWANDisconnect();
    if ((connected = ApiConnect())) {
      break;
    };
    usleep(kPollIntervalMs * 1000);
  }

  if (!connected) {
    // TODO(rochberg):  Send DeviceRemoved
    LOG(WARNING) << "Modem did not come back after reset";
    return false;
  } else {
    LOG(INFO) << "Modem returned from reset";
  }
  Enabled = true;
  return true;
}


bool GobiModem::ActivateOmadm() {
  ULONG rc;
  LOG(INFO) << "Activating OMA-DM";

  activation_state_ = -1;

  rc = sdk_->OMADMStartSession(gobi::kConfigure);
  if (rc != 0) {
    LOG(WARNING) << "OMADMStartSession failed: " << rc;
    return false;
  }

  // TODO(rochberg):  Factor out this pattern + add timeouts
  pthread_mutex_lock(&activation_mutex_);
  while (activation_state_ != gobi::kActivated) {
    LOG(WARNING) << "Waiting...";
    pthread_cond_wait(&activation_cond_, &activation_mutex_);
    if (activation_state_ <= gobi::kOmadmMaxFinal) {
      break;
    }
  }
  pthread_mutex_unlock(&activation_mutex_);
  return true;
}

bool GobiModem::ActivateOtasp(const std::string& number) {
  ULONG rc;

  rc = sdk_->GetActivationState(&activation_state_);
  LOG(INFO) << "Activation state: " << activation_state_;
  if (activation_state_ == gobi::kActivated) {
    return true;
  }

  if (activation_state_ != gobi::kNotActivated) {
    LOG(WARNING) << "Unexpected activation state: " << activation_state_;
    return false;
  }

  LOG(INFO) << "Activating";
  rc = sdk_->ActivateAutomatic(number.c_str());

  LOG(INFO) << "Waiting for activation to complete";
  // TODO(rochberg):  Timeout
  pthread_mutex_lock(&activation_mutex_);
  while (activation_state_ != gobi::kActivated) {
    pthread_cond_wait(&activation_cond_, &activation_mutex_);
  }
  pthread_mutex_unlock(&activation_mutex_);

  return ResetModem();
}

enum ActivationMethod {
  kActMethodOmadm,
  kActMethodOtasp,
  kActMethodNone
};

// TODO(rochberg):  Make this into a class, add dBM -> % maps,
// activation routines
struct Carrier {
  const char* name;
  const char* firmware_directory;
  ULONG carrier_id;
  int   carrier_type;
  int   activation_method;
  const char* activation_code;
};

static const Carrier kCarrierList[] = {
  // This is only a subset of the available carriers
  {"Vodafone", "0", 202, MM_MODEM_TYPE_GSM, kActMethodNone, NULL},
  {"Verizon Wireless", "1", 101, MM_MODEM_TYPE_CDMA, kActMethodOtasp, "*22899"},
  {"AT&T", "2", 201, MM_MODEM_TYPE_GSM, kActMethodNone, NULL},
  {"Sprint", "3", 102, MM_MODEM_TYPE_CDMA, kActMethodOmadm, NULL},
  {"T-Mobile", "4", 203, MM_MODEM_TYPE_GSM, kActMethodNone, NULL},
  {NULL, NULL, -1, -1},
};

static const Carrier *find_carrier(const char* carrier_name) {
  for (size_t i = 0; kCarrierList[i].name; ++i) {
    if (strcasecmp(carrier_name, kCarrierList[i].name) == 0) {
      return &kCarrierList[i];
    }
  }
  return NULL;
}

// TODO(rochberg):  Refactor this and build a Carrier class
bool GobiModem::DoActivation(const uint32_t method,
                             const char* number) {
  ULONG rc;

  rc = sdk_->GetActivationState(&activation_state_);
  LOG(INFO) << "Activation state: " << activation_state_;

  if (activation_state_ == gobi::kActivated) {
    return true;
  }

  if (activation_state_ != gobi::kNotActivated) {
    LOG(WARNING) << "Unexpected activation state: " << activation_state_;
    return false;
  }

  switch (method) {
  case kActMethodOmadm:
    return ActivateOmadm();
  case kActMethodOtasp:
    return ActivateOtasp(number);
  default:  // can't happen, our caller already validated args
    LOG(FATAL) << "Unexpected activation method: " << method;
    break;
  }
}

bool GobiModem::EnsureFirmwareLoaded(const char* carrier_name) {
  const Carrier *carrier = find_carrier(carrier_name);
  if (!carrier) {
    // TODO(rochberg):  Do we need to sanitize this string?
    LOG(WARNING) << "Could not parse carrier: " << carrier_name;
    return false;
  }

  ULONG rc;
  ULONG firmware_id;
  ULONG technology;
  ULONG modem_carrier_id;
  ULONG region;
  ULONG gps_capability;
  rc = sdk_->GetFirmwareInfo(&firmware_id,
                             &technology,
                             &modem_carrier_id,
                             &region,
                             &gps_capability);

  if (rc != 0) {
    LOG(WARNING) << "Could not get firmware info: " << rc;
    return false;
  }

  if (modem_carrier_id != carrier->carrier_id) {
    std::string image_path = "/opt/Qualcomm/Images2k/HP/";
    image_path += carrier->firmware_directory;

    LOG(INFO) << "Current Gobi carrier: " << modem_carrier_id
              << ".  Upgrading Gobi firmware with "
              << image_path;
    rc = UpgradeFirmware(const_cast<CHAR *>(image_path.c_str()));
    if (rc != 0) {
      LOG(WARNING) << "Firmware load from " << image_path << " failed: " << rc;
      return false;
    }

    if (!ResetModem()) {
      return false;
    }

    rc = sdk_->GetFirmwareInfo(&firmware_id,
                               &technology,
                               &modem_carrier_id,
                               &region,
                               &gps_capability);
    if (rc != 0) {
      LOG(WARNING) << "Could not get confirmation firmware info: " << rc;
      return false;
    }

    if (modem_carrier_id != carrier->carrier_id) {
      LOG(WARNING) << "After upgrade, expected carrier: " << carrier->carrier_id
                   << ".  Instead got: " << modem_carrier_id;
      return false;
    }
  }

  LOG(INFO) << "Carrier image selection complete: " << carrier_name;
  LogGobiInformation();
  return true;
}

void GobiModem::SetCarrier(const std::string& carrier, DBus::Error& error) {
  if (!EnsureFirmwareLoaded(carrier.c_str())) {
    // TODO(rochberg): ERROR
  }
}

void GobiModem::Activate(const std::string& carrier_name, DBus::Error& error) {
  LOG(INFO) << "Activate(" << carrier_name << ")";
  const Carrier *carrier = find_carrier(carrier_name.c_str());
  if (carrier == NULL) {
    LOG(WARNING) << "Unknown carrier";
    error.set(kErrorName, "unknown carrier");
    return;
  }
  ULONG rc;
#if 0
  // re-enable this when we know how to block until the
  // firmware load operation is complete
  if (!EnsureFirmwareLoaded(carrier->name)) {
    error.set(kErrorName, "loading firmware failed");
  }
#else

  // Check current firmware to see whether it's for the requested carrier
  ULONG firmware_id;
  ULONG technology;
  ULONG carrier_id;
  ULONG region;
  ULONG gps_capability;
  rc = sdk_->GetFirmwareInfo(&firmware_id,
                             &technology,
                             &carrier_id,
                             &region,
                             &gps_capability);

  if (rc != 0) {
    LOG(WARNING) << "Could not get firmware info: " << rc;
    error.set(kErrorName, "Error getting firmware info");
    return;
  }
  if (carrier_id != carrier->carrier_id) {
    LOG(WARNING) << "Current device firmware does not match the requested carrier.";
    LOG(WARNING) << "SetCarrier operation must be done before activating.";
    error.set(kErrorName, "Wrong carrier");
    return;
  }
#endif

  rc = sdk_->GetActivationState(&activation_state_);
  if (rc != 0) {
    LOG(WARNING) << "Cannot determine current activation state: " << rc;
    error.set(kErrorName, "Cannot determine current activation state");
    return;
  }
  LOG(INFO) << "Current activation state: " << activation_state_;
  if (activation_state_ == gobi::kActivated) {
    LOG(WARNING) << "Nothing to do: device is already activated";
    error.set(kErrorName, "Device is already activated");
    return;
  }

  switch (carrier->activation_method) {
    case kActMethodOmadm:    // OMA-DM
      DoActivation(carrier->activation_method, carrier->activation_code);
      break;

    case kActMethodOtasp:
      if (carrier->activation_code == NULL) {
        LOG(WARNING) << "Number was not supplied for OTASP activation";
        error.set(kErrorName, "No number supplied for OTASP activation");
        return;
      }
      DoActivation(carrier->activation_method, carrier->activation_code);
      break;

    default:
      LOG(WARNING) << "Unknown activation method: "
                   << carrier->activation_method;
      error.set(kErrorName, "Unknown activation method");
      break;
  }
}

void GobiModem::on_get_property(DBus::InterfaceAdaptor& interface,
                                const std::string& property,
                                DBus::Variant& value,
                                DBus::Error& error) {
  LOG(INFO) << "GetProperty called for " << property;
}


bool GobiModem::GetSignalStrengthDbm(int& output) {
  if (!Enabled()) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetSignalQuality on disabled modem";
    return false;
  }
  ULONG rc;
  ULONG kSignals = 10;
  ULONG signals = kSignals;
  INT8 strengths[kSignals];
  ULONG interfaces[kSignals];

  rc = sdk_->GetSignalStrengths(&signals, strengths, interfaces);
  if (rc != 0) {
    // TODO(rochberg):  ERROR
    LOG(WARNING) << "GetSignalStrengths failed: " << rc;
    return false;
  }
  signals = std::min(kSignals, signals);
  INT8 max_strength = -127;
  for (ULONG i = 0; i < signals; ++i) {
    LOG(INFO) << "Interface " << i << ": " << static_cast<int>(strengths[i])
              << " dBM technology: " << interfaces[i];
    // TODO(ers) mark radio interface technology as registered
    if (strengths[i] > max_strength) {
      max_strength = strengths[i];
    }
  }

  // If we're in the connected state, pick the signal strength for the radio
  // interface that's being used. Otherwise, pick the strongest signal.
  ULONG session_state;
  rc = sdk_->GetSessionState(&session_state);
  if (rc != 0) {
    // TODO(ers):  ERROR
    LOG(WARNING) << "GetSessionState failed: " << rc;
    return false;
  }
  if (session_state == gobi::kConnected) {
    ULONG db_technology;
    rc = GetDataBearerTechnology(&db_technology);
    if (rc != 0) {
      // TODO(ers):  ERROR
      LOG(WARNING) << "GetDataBearerTechnology failed: " << rc;
      return false;
    }
    ULONG rfi_technology = get_rfi_technology(db_technology);
    for (ULONG i = 0; i < signals; ++i) {
      if (interfaces[i] == rfi_technology) {
        signal_strength_ = strengths[i];
        output = strengths[i];
        return true;
      }
    }
  }

  output = max_strength;
  return true;
}

// Event callbacks:

void GobiModem::ActivationStatusCallback(ULONG activation_state) {
  LOG(INFO) << "Activation status callback: " << activation_state;
  pthread_mutex_lock(&activation_mutex_);
  activation_state_ = activation_state;
  pthread_cond_broadcast(&activation_cond_);
  pthread_mutex_unlock(&activation_mutex_);
}

void GobiModem::NmeaPlusCallback(const char *nmea, ULONG mode) {
  LOG(INFO) << "NMEA mode: " << mode << " " << nmea;
}

void GobiModem::OmadmStateCallback(ULONG session_state, ULONG failure_reason) {
  LOG(INFO) << "OMA-DM State Callback: "
            << session_state << " " << failure_reason;
  pthread_mutex_lock(&activation_mutex_);
  activation_state_ = session_state;
  pthread_cond_broadcast(&activation_cond_);
  pthread_mutex_unlock(&activation_mutex_);
}

void GobiModem::SignalStrengthCallback(INT8 signal_strength,
                                       ULONG radio_interface) {
  // translate dBm into percent
  unsigned long ss_percent =
      signal_strength_dbm_to_percent(signal_strength);
  signal_strength_ = signal_strength;
  // TODO(ers) only send DBus signal for "active" interface
  SignalQuality(ss_percent);
  LOG(INFO) << "Signal strength " << static_cast<int>(signal_strength)
            << " dBm on radio interface " << radio_interface
            << " (" << ss_percent << "%)";
  // TODO(ers) mark radio interface technology as registered?
}

void GobiModem::SessionStateCallback(ULONG state, ULONG session_end_reason) {
  LOG(INFO) << "SessionStateCallback " << state;
  if (state == gobi::kConnected)
    sdk_->GetDataBearerTechnology(&data_bearer_technology_);
  session_state_ = state;
  session_id_ = 0;
}

void GobiModem::UpdateRegistrationState(ULONG data_bearer_technology,
                                        ULONG roaming_state) {
  uint32_t cdma_1x_state;
  uint32_t evdo_state;
  uint32_t reg_state;

  if (roaming_state == gobi::kHome)
    reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  else
    reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;

  switch (data_bearer_technology) {
    case gobi::kDataBearerCdma1xRtt:
      cdma_1x_state = reg_state;
      break;

    case gobi::kDataBearerCdmaEvdo:
    case gobi::kDataBearerCdmaEvdoRevA:
      evdo_state = reg_state;
      break;

    default:
      cdma_1x_state = 0;
      evdo_state = 0;
      break;
  }
  data_bearer_technology_ = data_bearer_technology;
  roaming_state_ = roaming_state;
  RegistrationStateChanged(cdma_1x_state, evdo_state);
}

void GobiModem::DataBearerCallback(ULONG data_bearer_technology) {
  LOG(INFO) << "DataBearerCallback " << data_bearer_technology;

  UpdateRegistrationState(data_bearer_technology, roaming_state_);
}

void GobiModem::RoamingIndicatorCallback(ULONG roaming) {
  LOG(INFO) << "RoamingIndicatorCallback " << roaming;

  UpdateRegistrationState(data_bearer_technology_, roaming);
}
