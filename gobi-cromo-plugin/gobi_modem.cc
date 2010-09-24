// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem.h"
#include "gobi_modem_handler.h"

#include <fcntl.h>
#include <stdio.h>

extern "C" {
#include <libudev.h>
};

#include <cromo/cromo_server.h>

#include <glog/logging.h>
#include <mm/mm-modem.h>

static const size_t kDefaultBufferSize = 128;
static const char *kNetworkDriver = "QCUSBNet2k";
static const char *kFifoName = "/tmp/gobi-nmea";

#define DEFINE_ERROR(name) \
  static const char* k##name##Error = "org.chromium.ModemManager.Error." #name;
#define DEFINE_MM_ERROR(name) \
  static const char* k##name##Error = \
    "org.freedesktop.ModemManager.Modem.Gsm." #name;

DEFINE_ERROR(Activation)
DEFINE_ERROR(Activated)
DEFINE_ERROR(Connect)
DEFINE_ERROR(Disconnect)
DEFINE_ERROR(FirmwareLoad)
DEFINE_ERROR(Sdk)
DEFINE_ERROR(Mode)
DEFINE_ERROR(OperationInitiated)
DEFINE_MM_ERROR(NoNetwork)
DEFINE_MM_ERROR(OperationNotAllowed)

#define ENSURE_SDK_SUCCESS(function, rc, errtype)        \
    do {                                                 \
      if (rc != 0) {                                     \
        error.set(errtype, #function);                   \
        LOG(WARNING) << #function << " failed : " << rc; \
        return;                                          \
      }                                                  \
    } while (0)                                          \

#define ENSURE_SDK_SUCCESS_WITH_RESULT(function, rc, errtype, result) \
    do {                                                 \
      if (rc != 0) {                                     \
        error.set(errtype, #function);                   \
        LOG(WARNING) << #function << " failed : " << rc; \
        return result;                                   \
      }                                                  \
    } while (0)                                          \

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

static ULONG get_rfi_technology(ULONG data_bearer_technology) {
  switch (data_bearer_technology) {
    case gobi::kDataBearerCdma1xRtt:
      return gobi::kRfiCdma1xRtt;
    case gobi::kDataBearerCdmaEvdo:
    case gobi::kDataBearerCdmaEvdoRevA:
      return gobi::kRfiCdmaEvdo;
    case gobi::kDataBearerGprs:
      return gobi::kRfiGsm;
    case gobi::kDataBearerWcdma:
    case gobi::kDataBearerEdge:
    case gobi::kDataBearerHsdpaDlWcdmaUl:
    case gobi::kDataBearerWcdmaDlUsupaUl:
    case gobi::kDataBearerHsdpaDlHsupaUl:
      return gobi::kRfiUmts;
    default:
      return gobi::kRfiCdmaEvdo;
  }
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

static const Carrier *find_carrier(ULONG carrier_id) {
  for (size_t i = 0; kCarrierList[i].name; ++i) {
    if (kCarrierList[i].carrier_id == carrier_id) {
      return &kCarrierList[i];
    }
  }
  return NULL;
}

static struct udev_enumerate *enumerate_net_devices(struct udev *udev)
{
  int rc;
  struct udev_enumerate *udev_enumerate = udev_enumerate_new(udev);

  if (udev_enumerate == NULL) {
    return NULL;
  }

  rc = udev_enumerate_add_match_subsystem(udev_enumerate, "net");
  if (rc != 0) {
    udev_enumerate_unref(udev_enumerate);
    return NULL;
  }

  rc = udev_enumerate_scan_devices(udev_enumerate);
  if (rc != 0) {
    udev_enumerate_unref(udev_enumerate);
    return NULL;
  }
  return udev_enumerate;
}

GobiModem* GobiModem::connected_modem_(NULL);
GobiModemHandler* GobiModem::handler_;
GobiModem::mutex_wrapper_ GobiModem::modem_mutex_;

bool StartExitTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->StartExit();
}

bool ExitOkTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->ExitOk();
}

GobiModem::GobiModem(DBus::Connection& connection,
                     const DBus::Path& path,
                     const DEVICE_ELEMENT& device,
                     gobi::Sdk* sdk)
    : DBus::ObjectAdaptor(connection, path),
      sdk_(sdk),
      last_seen_(-1),
      nmea_fd_(-1),
      activation_state_(0),
      session_state_(gobi::kDisconnected),
      session_id_(0) {
  memcpy(&device_, &device, sizeof(device_));

  // Initialize DBus object properties
  SetDeviceProperties();
  SetModemProperties();
  Enabled = false;
  IpMethod = MM_MODEM_IP_METHOD_DHCP;
  UnlockRequired = "";
  UnlockRetries = 999;

  char name[64];
  snprintf(name, sizeof(name), "gobi-start-exit-%p", static_cast<void*>(this));
  handler_->server().start_exit_hooks().Add(name, StartExitTrampoline, this);
  snprintf(name, sizeof(name), "gobi-exit-ok-%p", static_cast<void*>(this));
  handler_->server().exit_ok_hooks().Add(name, ExitOkTrampoline, this);
}

GobiModem::~GobiModem() {
  char name[64];
  snprintf(name, sizeof(name), "gobi-start-exit-%p", static_cast<void*>(this));
  handler_->server().start_exit_hooks().Del(name);
  snprintf(name, sizeof(name), "gobi-exit-ok-%p", static_cast<void*>(this));
  handler_->server().exit_ok_hooks().Del(name);

  ApiDisconnect();
}

// DBUS Methods: Modem
void GobiModem::Enable(const bool& enable, DBus::Error& error) {
  LOG(INFO) << "Enable: " << Enabled() << " => " << enable;
  if (enable && !Enabled()) {
    ApiConnect(error);
    if (error.is_set())
      return;
    LogGobiInformation();
    ULONG firmware_id;
    ULONG technology;
    ULONG carrier_id;
    ULONG region;
    ULONG gps_capability;

    ULONG rc = sdk_->SetPower(gobi::kOnline);
    ENSURE_SDK_SUCCESS(SetPower, rc, kSdkError);
    rc = sdk_->GetFirmwareInfo(&firmware_id,
                               &technology,
                               &carrier_id,
                               &region,
                               &gps_capability);
    ENSURE_SDK_SUCCESS(GetFirmwareInfo, rc, kSdkError);

    const Carrier *carrier = find_carrier(carrier_id);

    if (carrier) {
      LOG(INFO) << "Current carrier is " << carrier->name
                << " (" << carrier_id << ")";
    } else {
      LOG(INFO) << "Current carrier is " << carrier_id;
    }
    Enabled = true;
    // TODO(ers) disabling NMEA thread creation
    // until issues are addressed, e.g., thread
    // leakage and possible invalid pointer references.
//    StartNMEAThread();
  } else if (!enable && Enabled()) {
    ULONG rc = sdk_->SetPower(gobi::kPersistentLowPower);
    if (rc != 0) {
      error.set(kSdkError, "SetPower");
      LOG(WARNING) << "SetPower failed : " << rc;
      return;
    }
    ApiDisconnect();
    Enabled = false;
  }
}

void GobiModem::Connect(const std::string& unused_number, DBus::Error& error) {
  if (!Enabled()) {
    LOG(WARNING) << "Connect on disabled modem";
    error.set(kConnectError, "Modem is disabled");
    return;
  }

  ULONG rc = sdk_->GetActivationState(&activation_state_);
  ENSURE_SDK_SUCCESS(GetActivationState, rc, kConnectError);

  LOG(INFO) << "Activation state: " << activation_state_;
  if (activation_state_ != gobi::kActivated) {
    LOG(WARNING) << "Connect failed because modem is not activated";
    error.set(kConnectError, "Modem is not activated");
    return;
  }

  ULONG failure_reason;

  for (int attempt = 0; attempt < 2; ++attempt) {
    LOG(INFO) << "Starting data session attempt " << attempt;
    rc = sdk_->StartDataSession(NULL,  // technology
                                NULL,  // APN Name
                                NULL,  // Authentication
                                NULL,  // Username
                                NULL,  // Password
                                &session_id_,  // OUT: session ID
                                &failure_reason  // OUT: failure reason
                                );
    if (rc == 0) {
      LOG(INFO) << "Session ID " <<  session_id_;
      error.set(kOperationInitiatedError, "");
      // session_state_ will be set in SessionStateCallback
      return;
    } else {
      LOG(WARNING) << "StartDataSession failed: " << rc;
      if (rc == gobi::kCallFailed) {
        LOG(WARNING) << "Failure Reason " <<  failure_reason;
        if (failure_reason == gobi::kClientEndedCall) {
          LOG(WARNING) <<
              "Call ended by client.  Retrying";
          continue;
        }
      }
      error.set(kConnectError, "StartDataSession");
      return;
    }
  }
  LOG(WARNING) << "Connect retries expired.  Failing";
}


void GobiModem::Disconnect(DBus::Error& error) {
  LOG(INFO) << "Disconnect(" << session_id_ << ")";
  if (session_state_ != gobi::kConnected) {
    LOG(WARNING) << "Disconnect called when not connected";
    error.set(kDisconnectError, "Not connected");
    return;
  }

  ULONG rc = sdk_->StopDataSession(session_id_);
  ENSURE_SDK_SUCCESS(StopDataSession, rc, kDisconnectError);
  error.set(kOperationInitiatedError, "");
  // session_state_ will be set in SessionStateCallback
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> GobiModem::GetIP4Config(
    DBus::Error& error) {
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  LOG(INFO) << "GetIP4Config (unimplemented)";

  return result;
}

void GobiModem::FactoryReset(const std::string& code, DBus::Error& error) {
  LOG(INFO) << "ResetToFactoryDefaults";
  ULONG rc = sdk_->ResetToFactoryDefaults(const_cast<CHAR *>(code.c_str()));
  ENSURE_SDK_SUCCESS(ResetToFactoryDefaults, rc, kSdkError);
  LOG(INFO) << "FactoryReset succeeded. Device should disappear and reappear.";
}

DBus::Struct<std::string, std::string, std::string> GobiModem::GetInfo(
    DBus::Error& error) {
  // (manufacturer, modem, version).
  DBus::Struct<std::string, std::string, std::string> result;

  char buffer[kDefaultBufferSize];

  ULONG rc = sdk_->GetManufacturer(sizeof(buffer), buffer);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetManufacturer, rc, kSdkError, result);
  result._1 = buffer;
  rc = sdk_->GetModelID(sizeof(buffer), buffer);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetModelID, rc, kSdkError, result);
  result._2 = buffer;
  rc = sdk_->GetFirmwareRevision(sizeof(buffer), buffer);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetFirmwareRevision, rc, kSdkError, result);
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

void GobiModem::GetSerialNumbers(SerialNumbers *out, DBus::Error &error) {
  char esn[kDefaultBufferSize];
  char imei[kDefaultBufferSize];
  char meid[kDefaultBufferSize];

  ULONG rc = sdk_->GetSerialNumbers(kDefaultBufferSize, esn,
                              kDefaultBufferSize, imei,
                              kDefaultBufferSize, meid);
  ENSURE_SDK_SUCCESS(GetSerialNumbers, rc, kSdkError);
  out->esn = esn;
  out->imei = imei;
  out->meid = meid;
}

DBusPropertyMap GobiModem::GetStatus(DBus::Error& error) {
  DBusPropertyMap result;

  // TODO(rochberg):  More mandatory properties expected
  // if we get SDK errors while trying to retrieve information,
  // we ignore them, and just don't set the corresponding properties
  int32_t rssi;
  DBus::Error signal_strength_error;

  StrengthMap interface_to_dbm;
  GetSignalStrengthDbm(rssi, &interface_to_dbm, signal_strength_error);
  if (!signal_strength_error.is_set()) {
    result["signal_strength_dbm"].writer().append_int32(rssi);
    for (StrengthMap::iterator i = interface_to_dbm.begin();
         i != interface_to_dbm.end();
         ++i) {
      char buf[30];
      snprintf(buf,
               sizeof(buf),
               "interface_%u_dbm",
               static_cast<unsigned int>(i->first));
      result[buf].writer().append_int32(i->second);
    }
  }

  SerialNumbers serials;
  // Distinct error because it invalid to modify an error once it is set
  DBus::Error serial_numbers_error;

  this->GetSerialNumbers(&serials, serial_numbers_error);
  if (!serial_numbers_error.is_set()) {
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
  char imsi[kDefaultBufferSize];
  ULONG rc = sdk_->GetIMSI(kDefaultBufferSize, imsi);
  if (rc == 0 && strlen(imsi) != 0)
    result["imsi"].writer().append_string(imsi);

  ULONG activation_state;
  rc = sdk_->GetActivationState(&activation_state);
  if (rc == 0)
    result["activation_state"].writer().append_uint32(activation_state);

  ULONG firmware_id;
  ULONG technology_id;
  ULONG carrier_id;
  ULONG region;
  ULONG gps_capability;
  rc = sdk_->GetFirmwareInfo(&firmware_id,
                             &technology_id,
                             &carrier_id,
                             &region,
                             &gps_capability);
  if (rc == 0) {
    const Carrier *carrier = find_carrier(carrier_id);
    const char* name;
    if (carrier != NULL)
      name = carrier->name;
    else
      name = "unknown";
    result["carrier"].writer().append_string(name);
    // TODO(ers) we'd like to return "operator_name", but the
    // SDK provides no apparent means of determining it.

    const char* technology;
    if (technology_id == 0)
      technology = "CDMA";
    else if (technology_id == 1)
      technology = "UMTS";
    else
      technology = "unknown";
    result["technology"].writer().append_string(technology);
  }
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
        ULONG l1, l2;      // don't care
        WORD w1, w2;
        BYTE b3[10];
        BYTE b2 = sizeof(b3)/sizeof(BYTE);
        rc = sdk_->GetServingNetwork(&reg_state, &l1, &b2, b3, &l2, &w1, &w2);
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
  GetSerialNumbers(&serials, error);
  return serials.esn;
}


uint32_t GobiModem::GetSignalQuality(DBus::Error& error) {
  if (!Enabled()) {
    LOG(WARNING) << "GetSignalQuality on disabled modem";
    error.set(kModeError, "Modem is disabled");
  } else {
    int32_t signal_strength_dbm;
    GetSignalStrengthDbm(signal_strength_dbm, NULL, error);
    if (!error.is_set()) {
      uint32_t result = signal_strength_dbm_to_percent(signal_strength_dbm);
      LOG(INFO) << "GetSignalQuality => " << result << "%";
      return result;
    }
  }
  // for the error cases, return an impossible value
  return 999;
}


// returns <band class, band, system id>
DBus::Struct<uint32_t, std::string, uint32_t> GobiModem::GetServingSystem(
    DBus::Error& error) {
  DBus::Struct<uint32_t, std::string, uint32_t> result;
  WORD mcc, mnc, sid, nid;
  CHAR netname[32];
  ULONG reg_state;
  ULONG roaming_state;
  ULONG l1;
  BYTE radio_interfaces[10];
  BYTE num_radio_interfaces = sizeof(radio_interfaces)/sizeof(BYTE);
  LOG(INFO) << "GetServingSystem";

  ULONG rc = sdk_->GetServingNetwork(&reg_state, &l1, &num_radio_interfaces,
                                     radio_interfaces, &roaming_state,
                                     &mcc, &mnc);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetServingNetwork, rc, kSdkError, result);
  if (reg_state != gobi::kRegistered) {
    error.set(kNoNetworkError, "No network service is available");
    return result;
  }

  rc = sdk_->GetHomeNetwork(&mcc, &mnc,
                            sizeof(netname), netname, &sid, &nid);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetHomeNetwork, rc, kSdkError, result);

  gobi::RfInfoInstance rf_info[10];
  BYTE rf_info_size = sizeof(rf_info) / sizeof(rf_info[0]);

  rc = sdk_->GetRFInfo(&rf_info_size, static_cast<BYTE*>((void *)&rf_info[0]));
  if (rc == gobi::kInformationElementUnavailable) {
    error.set(kNoNetworkError, "No network service is available");
    return result;
  } else if (rc != 0) {
    error.set(kSdkError, "GetRFInfo");
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
  ULONG reg_state;
  ULONG roaming_state;
  ULONG l1;
  WORD w1, w2;
  BYTE radio_interfaces[10];
  BYTE num_radio_interfaces = sizeof(radio_interfaces)/sizeof(BYTE);

  LOG(INFO) << "GetRegistrationState";
  cdma_1x_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  evdo_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;

  ULONG rc = sdk_->GetServingNetwork(&reg_state, &l1, &num_radio_interfaces,
                                     radio_interfaces, &roaming_state,
                                     &w1, &w2);
  ENSURE_SDK_SUCCESS(GetServingNetwork, rc, kSdkError);

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
    LOG(INFO) << "Registration state " << reg_state << " (" << mm_reg_state
              << ") for RFI " << (int)radio_interfaces[i];
    if (radio_interfaces[i] == gobi::kRfiCdma1xRtt)
      cdma_1x_state = mm_reg_state;
    else if (radio_interfaces[i] == gobi::kRfiCdmaEvdo)
      evdo_state = mm_reg_state;
  }
  LOG(INFO) << "  => 1xRTT: " << cdma_1x_state << " EVDO: " << evdo_state;
}

static void ByteTotalsCallback(ULONGLONG tx, ULONGLONG rx) {
  LOG(INFO) << "ByteTotalsCallback: tx " << tx << " rx " << rx;
}

static void DataCapabilitiesCallback(BYTE size, BYTE* caps) {
  ULONG *ucaps = reinterpret_cast<ULONG*>(caps);
  LOG(INFO) << "DataCapabiliesHandler: " << size << " caps";
  for (int i = 0; i < size; i++) {
    LOG(INFO) << "Cap: " << ucaps[i];
  }
}

static void DormancyStatusCallback(ULONG status) {
  LOG(INFO) << "DormancyStatusCallback: " << status;
}

static void MobileIPStatusCallback(ULONG status) {
  LOG(INFO) << "MobileIPStatusCallback: " << status;
}

static void PowerCallback(ULONG mode) {
  LOG(INFO) << "PowerCallback: " << mode;
}

static void RFInfoCallback(ULONG iface, ULONG bandclass, ULONG channel) {
  LOG(INFO) << "RFInfoCallback: iface " << iface
            << " bandclass " << bandclass
            << " channel " << channel;
}

static void LURejectCallback(ULONG domain, ULONG cause) {
  LOG(INFO) << "LURejectCallback: domain " << domain << " cause " << cause;
}

static void NewSMSCallback(ULONG type, ULONG index) {
  LOG(INFO) << "NewSMSCallback: type " << type << " index " << index;
}

static void NMEACallback(LPCSTR nmea) {
  LOG(INFO) << "NMEACallback: " << nmea;
}

static void PDSStateCallback(ULONG enabled, ULONG tracking) {
  LOG(INFO) << "PDSStateCallback: enabled " << enabled
            << " tracking " << tracking;
}

static void OMADMAlertCallback(ULONG type, USHORT id) {
  LOG(INFO) << "OMDADMAlertCallback type " << type << " id " << id;
}

void GobiModem::RegisterCallbacks() {
  sdk_->SetByteTotalsCallback(ByteTotalsCallback, 1);
  sdk_->SetDataCapabilitiesCallback(DataCapabilitiesCallback);
  sdk_->SetDormancyStatusCallback(DormancyStatusCallback);
  sdk_->SetMobileIPStatusCallback(MobileIPStatusCallback);
  sdk_->SetPowerCallback(PowerCallback);
  sdk_->SetRFInfoCallback(RFInfoCallback);
  sdk_->SetLURejectCallback(LURejectCallback);
  sdk_->SetNewSMSCallback(NewSMSCallback);
  sdk_->SetNMEACallback(NMEACallback);
  sdk_->SetPDSStateCallback(PDSStateCallback);
  sdk_->SetOMADMAlertCallback(OMADMAlertCallback);

  sdk_->SetActivationStatusCallback(ActivationStatusCallbackTrampoline);
  sdk_->SetNMEAPlusCallback(NmeaPlusCallbackTrampoline);
  sdk_->SetOMADMStateCallback(OmadmStateCallbackTrampoline);
  sdk_->SetSessionStateCallback(SessionStateCallbackTrampoline);
  sdk_->SetDataBearerCallback(DataBearerCallbackTrampoline);
  sdk_->SetRoamingIndicatorCallback(RoamingIndicatorCallbackTrampoline);

  static int num_thresholds = kSignalStrengthNumLevels - 1;
  int interval =
      (kMaxSignalStrengthDbm - kMinSignalStrengthDbm) /
      (kSignalStrengthNumLevels - 1);
  INT8 thresholds[num_thresholds];
  for (int i = 0; i < num_thresholds; i++) {
    thresholds[i] = kMinSignalStrengthDbm + interval * i;
  }
  sdk_->SetSignalStrengthCallback(SignalStrengthCallbackTrampoline,
                                  num_thresholds,
                                  thresholds);
}

void GobiModem::ApiConnect(DBus::Error& error) {

  // It is safe to test for NULL outside of a lock because ApiConnect
  // is only called by the main thread, and only the main thread can
  // modify connected_modem_.
  if (connected_modem_ != NULL) {
    LOG(INFO) << "ApiAlready connected: connected_modem_=0x" << connected_modem_
              << "this=0x" << this;
    error.set(kOperationNotAllowedError,
              "Only one modem can be connected via Api");
    return;
  }
  ULONG rc = sdk_->QCWWANConnect(device_.deviceNode, device_.deviceKey);
  ENSURE_SDK_SUCCESS(QCWWANConnect, rc, kSdkError);
  pthread_mutex_lock(&modem_mutex_.mutex_);
  connected_modem_ = this;
  pthread_mutex_unlock(&modem_mutex_.mutex_);
  RegisterCallbacks();
}


ULONG GobiModem::ApiDisconnect(void) {

  ULONG rc = 0;

  pthread_mutex_lock(&modem_mutex_.mutex_);
  if (connected_modem_ == this) {
    LOG(INFO) << "Disconnecting from QCWWAN.  this_=0x" << this;
    connected_modem_ = NULL;
    pthread_mutex_unlock(&modem_mutex_.mutex_);
    rc = sdk_->QCWWANDisconnect();
  } else {
    LOG(INFO) << "Not connected.  this_=0x" << this;
    pthread_mutex_unlock(&modem_mutex_.mutex_);
  }

  return rc;
}

void GobiModem::LogGobiInformation() {
  ULONG rc;

  char buffer[kDefaultBufferSize];
  rc = sdk_->GetManufacturer(sizeof(buffer), buffer);
  if (rc == 0) {
    LOG(INFO) << "Manufacturer: " << buffer;
  }

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
  if (rc == 0) {
    LOG(INFO) << "Firmware info: "
              << "firmware_id: " << firmware_id
              << " technology: " << technology
              << " carrier: " << carrier
              << " region: " << region
              << " gps_capability: " << gps_capability;
  } else {
    LOG(WARNING) << "Cannot get firmware info: " << rc;
  }

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
    LOG(WARNING) << "Cannot get firmware revision info: " << rc;
  }

  rc = sdk_->GetImageStore(sizeof(buffer), buffer);
  if (rc == 0) {
    LOG(INFO) << "ImageStore: " << buffer;
  } else {
    LOG(WARNING) << "Cannot get image store info: " << rc;
  }

  SerialNumbers serials;
  DBus::Error error;
  GetSerialNumbers(&serials, error);
  if (!error.is_set()) {
    LOG(INFO) << "ESN: " << serials.esn;
    LOG(INFO) << "IMEI: " << serials.imei;
    LOG(INFO) << "MEID: " << serials.meid;
  } else {
    LOG(WARNING) << "Cannot get serial numbers: " << error;
  }

  char number[kDefaultBufferSize], min_array[kDefaultBufferSize];
  rc = sdk_->GetVoiceNumber(kDefaultBufferSize, number,
                            kDefaultBufferSize, min_array);
  if (rc == 0) {
    LOG(INFO) << "Voice: " << number
              << " MIN: " << min_array;
  } else if (rc != gobi::kNotProvisioned) {
    LOG(WARNING) << "GetVoiceNumber failed: " << rc;
  }

  BYTE index;
  rc = sdk_->GetActiveMobileIPProfile(&index);
  if (rc != 0 && rc != gobi::kNotSupportedByDevice) {
    LOG(WARNING) << "GetAMIPP: " << rc;
  } else {
    LOG(INFO) << "Mobile IP profile: " << (int)index;
  }
}

void GobiModem::SoftReset(DBus::Error& error) {
  ResetModem(error);
}

void GobiModem::PowerCycle(DBus::Error &error) {
  LOG(INFO) << "Initiating modem powercycle";
  ULONG rc = sdk_->SetPower(gobi::kPowerOff);
  ENSURE_SDK_SUCCESS(SetPower, rc, kSdkError);
}

void GobiModem::ResetModem(DBus::Error& error) {
  // TODO(rochberg):  Is this going to confuse connman?
  Enabled = false;
  LOG(INFO) << "Offline";

  // Resetting the modem will cause it to disappear and reappear.
  ULONG rc = sdk_->SetPower(gobi::kOffline);
  ENSURE_SDK_SUCCESS(SetPower, rc, kSdkError);

  LOG(INFO) << "Reset";
  rc = sdk_->SetPower(gobi::kReset);
  ENSURE_SDK_SUCCESS(SetPower, rc, kSdkError);

  rc = ApiDisconnect();
  ENSURE_SDK_SUCCESS(QCWWanDisconnect, rc, kSdkError);
}

// pre-condition: activation_state_ == gobi::kConnecting
void GobiModem::ActivateOmadm(DBus::Error& error) {
  ULONG rc;
  char errmsg[64];

  LOG(INFO) << "Activating OMA-DM";

  rc = sdk_->OMADMStartSession(gobi::kConfigure);
  if (rc != 0) {
    activation_state_ = gobi::kNotActivated;
    snprintf(errmsg, sizeof(errmsg), "OMA-DM returned: %ld", rc);
    error.set(kActivationError, errmsg);
    LOG(ERROR) << "OMA-DM activation failed: " << rc;
    return;
  }
}

void GobiModem::ActivateOtasp(const std::string& number, DBus::Error& error) {
  ULONG rc;
  char errmsg[64];

  LOG(INFO) << "Activating OTASP";

  rc = sdk_->ActivateAutomatic(number.c_str());
  if (rc != 0) {
    activation_state_ = gobi::kNotActivated;
    snprintf(errmsg, sizeof(errmsg), "OTASP returned: %ld", rc);
    error.set(kActivationError, errmsg);
    LOG(ERROR) << "OTASP activation failed: " << rc;
    return;
   }
}

void GobiModem::SetCarrier(const std::string& carrier_name,
                           DBus::Error& error) {
  const Carrier *carrier = find_carrier(carrier_name.c_str());
  if (!carrier) {
    // TODO(rochberg):  Do we need to sanitize this string?
    LOG(WARNING) << "Could not parse carrier: " << carrier_name;
    error.set(kFirmwareLoadError, "Unknown carrier name");
    return;
  }

  LOG(INFO) << "Carrier image selection starting: " << carrier_name;
  ULONG firmware_id;
  ULONG technology;
  ULONG modem_carrier_id;
  ULONG region;
  ULONG gps_capability;
  ULONG rc = sdk_->GetFirmwareInfo(&firmware_id,
                                   &technology,
                                   &modem_carrier_id,
                                   &region,
                                   &gps_capability);
  ENSURE_SDK_SUCCESS(GetFirmwareInfo, rc, kFirmwareLoadError);

  if (modem_carrier_id != carrier->carrier_id) {

    // UpgradeFirmware doesn't pay attention to anything before the
    // last /, so we don't supply it
    std::string image_path = std::string("/") + carrier->firmware_directory;

    LOG(INFO) << "Current Gobi carrier: " << modem_carrier_id
              << ".  Carrier image selection of "
              << image_path;
    rc = sdk_->UpgradeFirmware(const_cast<CHAR *>(image_path.c_str()));
    if (rc != 0) {
      LOG(WARNING) << "Carrier image selection from: "
                   << image_path << " failed: " << rc;
      error.set(kFirmwareLoadError, "UpgradeFirmware");
    } else {
      ApiDisconnect();
    }
  }
}

void GobiModem::Activate(const std::string& carrier_name, DBus::Error& error) {
  LOG(INFO) << "Activate(" << carrier_name << ")";

  // Check current firmware to see whether it's for the requested carrier
  ULONG firmware_id;
  ULONG technology;
  ULONG carrier_id;
  ULONG region;
  ULONG gps_capability;

  ULONG rc = sdk_->GetFirmwareInfo(&firmware_id,
                                   &technology,
                                   &carrier_id,
                                   &region,
                                   &gps_capability);
  ENSURE_SDK_SUCCESS(GetFirmwareInfo, rc, kFirmwareLoadError);

  const Carrier *carrier;
  if (carrier_name.empty()) {
    carrier = find_carrier(carrier_id);
    if (carrier == NULL) {
      LOG(WARNING) << "Unknown carrier id: " << carrier_id;
      error.set(kActivationError, "Unknown carrier");
      return;
    }
  } else {
    carrier = find_carrier(carrier_name.c_str());
    if (carrier == NULL) {
      LOG(WARNING) << "Unknown carrier: " << carrier_name;
      error.set(kActivationError, "Unknown carrier");
      return;
    }
    if (carrier_id != carrier->carrier_id) {
      LOG(WARNING) << "Current device firmware does not match the requested carrier.";
      LOG(WARNING) << "SetCarrier operation must be done before activating.";
      error.set(kActivationError, "Wrong carrier");
      return;
    }
  }

  rc = sdk_->GetActivationState(&activation_state_);
  ENSURE_SDK_SUCCESS(GetActivationState, rc, kActivationError);

  LOG(INFO) << "Current activation state: " << activation_state_;
  if (activation_state_ == gobi::kActivated) {
    LOG(WARNING) << "Nothing to do: device is already activated";
    error.set(kActivatedError, "Device is already activated");
    return;
  }

  if (activation_state_ != gobi::kNotActivated) {
    LOG(WARNING) << "Unexpected activation state: " << activation_state_;
    error.set(kActivationError, "Unexpected activation state");
    return;
  }

  activation_state_ = gobi::kActivationConnecting;

  switch (carrier->activation_method) {
    case kActMethodOmadm:
      ActivateOmadm(error);
      break;

    case kActMethodOtasp:
      if (carrier->activation_code == NULL) {
        LOG(WARNING) << "Number was not supplied for OTASP activation";
        activation_state_ = gobi::kNotActivated;
        error.set(kActivationError, "No number supplied for OTASP activation");
        return;
      }
      ActivateOtasp(carrier->activation_code, error);
      break;

    default:
      LOG(WARNING) << "Unknown activation method: "
                   << carrier->activation_method;
      activation_state_ = gobi::kNotActivated;
      error.set(kActivationError, "Unknown activation method");
      break;
  }
  if (!error.is_set())
    error.set(kOperationInitiatedError, "");
}

void GobiModem::on_get_property(DBus::InterfaceAdaptor& interface,
                                const std::string& property,
                                DBus::Variant& value,
                                DBus::Error& error) {
  LOG(INFO) << "GetProperty called for " << property;
}


void GobiModem::GetSignalStrengthDbm(int& output,
                                     StrengthMap *interface_to_dbm,
                                     DBus::Error& error) {
  ULONG kSignals = 10;
  ULONG signals = kSignals;
  INT8 strengths[kSignals];
  ULONG interfaces[kSignals];

  ULONG rc = sdk_->GetSignalStrengths(&signals, strengths, interfaces);
  ENSURE_SDK_SUCCESS(GetSignalStrengths, rc, kSdkError);

  signals = std::min(kSignals, signals);

  if (interface_to_dbm) {
    for (ULONG i = 0; i < signals; ++i) {
      (*interface_to_dbm)[interfaces[i]] = strengths[i];
    }
  }

  INT8 max_strength = -127;
  for (ULONG i = 0; i < signals; ++i) {
    LOG(INFO) << "Interface " << i << ": " << static_cast<int>(strengths[i])
              << " dBM technology: " << interfaces[i];
    // TODO(ers) mark radio interface technology as registered?
    if (strengths[i] > max_strength) {
      max_strength = strengths[i];
    }
  }

  // If we're in the connected state, pick the signal strength for the radio
  // interface that's being used. Otherwise, pick the strongest signal.
  ULONG session_state;
  rc = sdk_->GetSessionState(&session_state);
  ENSURE_SDK_SUCCESS(GetSessionState, rc, kSdkError);

  if (session_state == gobi::kConnected) {
    ULONG db_technology;
    rc =  sdk_->GetDataBearerTechnology(&db_technology);
    if (rc != 0) {
      LOG(WARNING) << "GetDataBearerTechnology failed: " << rc;
      error.set(kSdkError, "GetDataBearerTechnology");
      return;
    }
    ULONG rfi_technology = get_rfi_technology(db_technology);
    for (ULONG i = 0; i < signals; ++i) {
      if (interfaces[i] == rfi_technology) {
        output = strengths[i];
        return;
      }
    }
  }
  output = max_strength;
}

// Set properties for which a connection to the SDK is required
// to obtain the needed information. Since this is called before
// the modem is enabled, we connect to the SDK, get the properties
// we need, and then disconnect from the SDK.
// pre-condition: Enabled == false
void GobiModem::SetModemProperties() {
  DBus::Error connect_error;

  ApiConnect(connect_error);
  if (connect_error.is_set()) {
    // Use a default identifier assuming a single GOBI is connected
    EquipmentIdentifier = "GOBI";
    Type = MM_MODEM_TYPE_CDMA;
    return;
  }

  SerialNumbers serials;
  DBus::Error getserial_error;
  GetSerialNumbers(&serials, getserial_error);
  DBus::Error getdev_error;
  ULONG u1, u2, u3, u4;
  BYTE radioInterfaces[10];
  ULONG numRadioInterfaces = sizeof(radioInterfaces)/sizeof(BYTE);
  ULONG rc = sdk_->GetDeviceCapabilities(&u1, &u2, &u3, &u4,
                                         &numRadioInterfaces,
                                         radioInterfaces);
  if (rc == 0) {
    if (numRadioInterfaces != 0) {
      if (radioInterfaces[0] == gobi::kRfiGsm ||
          radioInterfaces[0] == gobi::kRfiUmts) {
        Type = MM_MODEM_TYPE_GSM;
      } else {
        Type = MM_MODEM_TYPE_CDMA;
      }
    }
  }
  ApiDisconnect();
  if (!getserial_error.is_set()) {
    // TODO(jglasgow): if GSM return serials.imei
    EquipmentIdentifier = serials.meid;
    MEID = serials.meid;
  } else {
    // Use a default identifier assuming a single GOBI is connected
    EquipmentIdentifier = "GOBI";
  }
}

void *GobiModem::NMEAThread(void) {
  int fd;
  ULONG gps_mask, cell_mask;

  unlink(kFifoName);
  mkfifo(kFifoName, 0700);

  // This will wait for a reader to open before continuing
  fd = open(kFifoName, O_WRONLY);

  LOG(INFO) << "NMEA fifo running, GPS enabled";

  // Enable GPS tracking
  sdk_->SetServiceAutomaticTracking(1);

  // Reset all GPS/Cell positioning fields at startup
  gps_mask = 0x1fff;
  cell_mask = 0x3ff;
  sdk_->ResetPDSData(&gps_mask, &cell_mask);

  nmea_fd_ = fd;

  return NULL;
}

void GobiModem::StartNMEAThread() {
  // Create thread to wait for fifo reader
  pthread_create(&nmea_thread, NULL, NMEAThreadTrampoline, NULL);
}

// Event callbacks run in the context of the main thread

gboolean GobiModem::NmeaPlusCallback(gpointer data) {
  NmeaPlusArgs *args = static_cast<NmeaPlusArgs*>(data);
  LOG(INFO) << "NMEA Plus State Callback: "
            << args->nmea << " " << args->mode;
  GobiModem* modem = handler_->LookupByPath(*args->path);
  if (modem != NULL && modem->nmea_fd_ != -1) {
    int ret = write(modem->nmea_fd_, args->nmea.c_str(), args->nmea.length());
    if (ret < 0) {
      // Failed write means fifo reader went away
      LOG(INFO) << "NMEA fifo stopped, GPS disabled";
      unlink(kFifoName);
      close(modem->nmea_fd_);
      modem->nmea_fd_ = -1;

      // Disable GPS tracking until we have a listener again
      modem->sdk_->SetServiceAutomaticTracking(0);

      // Re-start the fifo listener thread
      modem->StartNMEAThread();
    }
  }
  delete args;
  return FALSE;
}

gboolean GobiModem::OmadmStateCallback(gpointer data) {
  OmadmStateArgs* args = static_cast<OmadmStateArgs*>(data);
  LOG(INFO) << "OMA-DM State Callback: " << args->session_state;
  GobiModem* modem = handler_->LookupByPath(*args->path);
  if (modem != NULL) {
    switch (args->session_state) {
      case gobi::kOmadmComplete:
        modem->activation_state_ = gobi::kActivated;
        modem->ActivationCompleted(true);
        break;
      case gobi::kOmadmFailed:
        LOG(INFO) << "OMA-DM failure reason: " << args->failure_reason;
        // fall through
      case gobi::kOmadmUpdateInformationUnavailable:
        modem->activation_state_ = gobi::kNotActivated;
        modem->ActivationCompleted(false);
        break;
    }
  }
  delete args;
  return FALSE;
}

gboolean GobiModem::ActivationStatusCallback(gpointer data) {
  ActivationStatusArgs* args = static_cast<ActivationStatusArgs*>(data);
  LOG(INFO) << "Activation status callback: " << args->activation_state;
  GobiModem* modem = handler_->LookupByPath(*args->path);
  if (modem != NULL) {
    modem->activation_state_ = args->activation_state;
    if (args->activation_state == gobi::kActivated ||
        args->activation_state == gobi::kNotActivated)
      modem->ActivationCompleted(args->activation_state == gobi::kActivated);
  }
  // TODO(ers) The SDK documentation says that before we can signal that
  // activation is complete, we need to power off the device and call
  // QCWWANDisconnect().
  delete args;
  return FALSE;
}

// Runs in the context of the main thread
gboolean GobiModem::SignalStrengthCallback(gpointer data) {
  SignalStrengthArgs* args = static_cast<SignalStrengthArgs*>(data);
  GobiModem* modem = handler_->LookupByPath(*args->path);
  if (modem != NULL)
    modem->SignalStrengthHandler(args->signal_strength, args->radio_interface);
  delete args;
  return FALSE;
}

gboolean GobiModem::SessionStateCallback(gpointer data) {
  SessionStateArgs* args = static_cast<SessionStateArgs*>(data);
  GobiModem* modem = handler_->LookupByPath(*args->path);
  if (modem != NULL)
    modem->SessionStateHandler(args->state, args->session_end_reason);
  delete args;
  return FALSE;
}

gboolean GobiModem::RegistrationStateCallback(gpointer data) {
  CallbackArgs* args = static_cast<CallbackArgs*>(data);
  GobiModem* modem = handler_->LookupByPath(*args->path);
  delete args;
  if (modem != NULL)
    modem->RegistrationStateHandler();
  return FALSE;
}

void GobiModem::SignalStrengthHandler(INT8 signal_strength,
                                      ULONG radio_interface) {
  // translate dBm into percent
  unsigned long ss_percent =
      signal_strength_dbm_to_percent(signal_strength);
  // TODO(ers) only send DBus signal for "active" interface
  SignalQuality(ss_percent);
  LOG(INFO) << "SignalStrengthHandler " << static_cast<int>(signal_strength)
            << " dBm on radio interface " << radio_interface
            << " (" << ss_percent << "%)";
  // TODO(ers) mark radio interface technology as registered?
}

void GobiModem::SessionStateHandler(ULONG state, ULONG session_end_reason) {
  LOG(INFO) << "SessionStateHandler " << state;
  if (state == gobi::kConnected) {
    ULONG data_bearer_technology;
    sdk_->GetDataBearerTechnology(&data_bearer_technology);
    // TODO(ers) send a signal or change a property to notify
    // listeners about the change in data bearer technology
  }

  session_state_ = state;
  if (state == gobi::kDisconnected)
    session_id_ = 0;
  bool is_connected = (state == gobi::kConnected);
  ConnectionStateChanged(is_connected, QMIReasonToMMReason(session_end_reason));
}

void GobiModem::RegistrationStateHandler() {
  uint32_t cdma_1x_state;
  uint32_t evdo_state;
  DBus::Error error;

  LOG(INFO) << "RegistrationStateHandler";
  GetRegistrationState(cdma_1x_state, evdo_state, error);
  if (error.is_set())
    return;
  RegistrationStateChanged(cdma_1x_state, evdo_state);
  if (session_state_ == gobi::kConnected) {
    ULONG data_bearer_technology;
    sdk_->GetDataBearerTechnology(&data_bearer_technology);
    // TODO(ers) send a signal or change a property to notify
    // listeners about the change in data bearer technology
  }
}

// Set DBus properties that pertain to the modem hardware device.
// The properties set here are Device, MasterDevice, and Driver.
void GobiModem::SetDeviceProperties()
{
  struct udev *udev = udev_new();

  if (udev == NULL) {
    LOG(WARNING) << "udev == NULL";
    return;
  }

  struct udev_list_entry *entry;
  struct udev_enumerate *udev_enumerate = enumerate_net_devices(udev);
  if (udev_enumerate == NULL) {
    LOG(WARNING) << "udev_enumerate == NULL";
    udev_unref(udev);
    return;
  }

  for (entry = udev_enumerate_get_list_entry(udev_enumerate);
      entry != NULL;
      entry = udev_list_entry_get_next(entry)) {

    std::string syspath(udev_list_entry_get_name(entry));

    struct udev_device *udev_device =
        udev_device_new_from_syspath(udev, syspath.c_str());
    if (udev_device == NULL)
      continue;

    std::string driver;
    struct udev_device *parent = udev_device_get_parent(udev_device);
    if (parent != NULL)
      driver = udev_device_get_driver(parent);

    if (driver.compare(kNetworkDriver) == 0) {
      // Extract last portion of syspath...
      size_t found = syspath.find_last_of('/');
      if (found != std::string::npos) {
        Device = syspath.substr(found + 1);
        struct udev_device *grandparent;
        if (parent != NULL) {
          grandparent = udev_device_get_parent(parent);
          if (grandparent != NULL)
            MasterDevice = udev_device_get_syspath(grandparent);
        }
        Driver = driver;
        udev_device_unref(udev_device);

        // TODO(jglasgow): Support multiple devices.
        // This functions returns the first network device whose
        // driver is a qualcomm network device driver.  This will not
        // work properly if a machine has multiple devices that use the
        // Qualcomm network device driver.
        break;
      }
    }
    udev_device_unref(udev_device);
  }
  udev_enumerate_unref(udev_enumerate);
  udev_unref(udev);
}

bool GobiModem::StartExit() {
  if (session_id_)
    sdk_->StopDataSession(session_id_);
  return true;
}

bool GobiModem::ExitOk() {
  return session_id_ == 0;
}

unsigned int GobiModem::QMIReasonToMMReason(unsigned int qmireason) {
  switch (qmireason) {
    case gobi::kClientEndedCall:
      return MM_MODEM_CONNECTION_STATE_CHANGE_REASON_REQUESTED;
    default:
      return MM_MODEM_CONNECTION_STATE_CHANGE_REASON_UNKNOWN;
  }
}
