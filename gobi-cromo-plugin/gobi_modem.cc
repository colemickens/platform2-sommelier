// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem.h"
#include "gobi_modem_handler.h"
#include <sstream>

#include <dbus/dbus.h>

#include <fcntl.h>
#include <stdio.h>

extern "C" {
#include <libudev.h>
#include <time.h>
};

#include <base/stringprintf.h>
#include <cromo/carrier.h>
#include <cromo/cromo_server.h>
#include <cromo/utilities.h>

#include <glog/logging.h>
#include <mm/mm-modem.h>

// This ought to be in a header file somewhere, but if it is, I can't find it.
#ifndef NDEBUG
static const int DEBUG = 1;
#else
static const int DEBUG = 0;
#endif

static const size_t kDefaultBufferSize = 128;
static const char *kNetworkDriver = "QCUSBNet2k";
static const char *kFifoName = "/tmp/gobi-nmea";

using utilities::DBusPropertyMap;

#define DEFINE_ERROR(name) \
  static const char* k##name##Error = "org.chromium.ModemManager.Error." #name;
#define DEFINE_MM_ERROR(name) \
  static const char* k##name##Error = \
    "org.freedesktop.ModemManager.Modem.Gsm." #name;

DEFINE_ERROR(Activation)
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

// Returns the current time, in milliseconds, from an unspecified epoch.
static unsigned long long time_ms(void) {
  struct timespec ts;
  unsigned long long rv;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  rv = ts.tv_sec;
  rv *= 1000;
  rv += ts.tv_nsec / 1000000;
  return rv;
}

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
  return modem->is_disconnected();
}

bool SuspendOkTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->is_disconnected();
}

GobiModem::GobiModem(DBus::Connection& connection,
                     const DBus::Path& path,
                     const DEVICE_ELEMENT& device,
                     gobi::Sdk* sdk)
    : DBus::ObjectAdaptor(connection, path),
      sdk_(sdk),
      last_seen_(-1),
      nmea_fd_(-1),
      session_state_(gobi::kDisconnected),
      session_id_(0),
      suspending_(false) {
  memcpy(&device_, &device, sizeof(device_));

  metrics_lib_.reset(new MetricsLibrary());
  metrics_lib_->Init();

  // Initialize DBus object properties
  SetDeviceProperties();
  SetModemProperties();
  Enabled = false;
  IpMethod = MM_MODEM_IP_METHOD_DHCP;
  UnlockRequired = "";
  UnlockRetries = 999;

  carrier_ = handler_->server().FindCarrierNoOp();

  char name[64];
  void *thisp = static_cast<void*>(this);
  snprintf(name, sizeof(name), "gobi-%p", thisp);
  hooks_name_ = name;

  handler_->server().start_exit_hooks().Add(hooks_name_, StartExitTrampoline,
                                            this);
  handler_->server().exit_ok_hooks().Add(hooks_name_, ExitOkTrampoline, this);
  RegisterStartSuspend(hooks_name_);
  handler_->server().suspend_ok_hooks().Add(hooks_name_, SuspendOkTrampoline,
                                            this);
}

GobiModem::~GobiModem() {
  handler_->server().start_exit_hooks().Del(hooks_name_);
  handler_->server().exit_ok_hooks().Del(hooks_name_);
  handler_->server().UnregisterStartSuspend(hooks_name_);
  handler_->server().suspend_ok_hooks().Del(hooks_name_);

  ApiDisconnect();
}

enum {
  METRIC_INDEX_NONE = 0,
  METRIC_INDEX_QMI_HARDWARE_RESTRICTED,
  METRIC_INDEX_MAX
};

static unsigned int QCErrorToMetricIndex(unsigned int error) {
  if (error == gobi::kQMIHardwareRestricted)
    return METRIC_INDEX_QMI_HARDWARE_RESTRICTED;
  return METRIC_INDEX_NONE;
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
    metrics_lib_->SendEnumToUMA("Network.3G.Gobi.SetPower",
                                QCErrorToMetricIndex(rc),
                                METRIC_INDEX_MAX);
    ENSURE_SDK_SUCCESS(SetPower, rc, kSdkError);
    rc = sdk_->GetFirmwareInfo(&firmware_id,
                               &technology,
                               &carrier_id,
                               &region,
                               &gps_capability);
    ENSURE_SDK_SUCCESS(GetFirmwareInfo, rc, kSdkError);

    const Carrier *carrier =
        handler_->server().FindCarrierByCarrierId(carrier_id);

    if (carrier != NULL) {
      LOG(INFO) << "Current carrier is " << carrier->name()
                << " (" << carrier->carrier_id() << ")";
      carrier_ = carrier;
    } else {
      LOG(WARNING) << "Unknown carrier, id=" << carrier_id;
    }
    Enabled = true;
    registration_start_time_ = time_ms();
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
  ULONG failure_reason;
  ULONG rc;
  for (int attempt = 0; attempt < 2; ++attempt) {
    LOG(INFO) << "Starting data session attempt " << attempt;
    connect_start_time_ = time_ms();
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

  disconnect_start_time_ = time_ms();
  ULONG rc = sdk_->StopDataSession(session_id_);
  if (rc != 0)
    disconnect_start_time_ = 0;
  ENSURE_SDK_SUCCESS(StopDataSession, rc, kDisconnectError);
  error.set(kOperationInitiatedError, "");
  // session_state_ will be set in SessionStateCallback
}

std::string GobiModem::GetUSBAddress() {
  return sysfs_path_.substr(sysfs_path_.find_last_of('/') + 1);
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
  rc = sdk_->GetHardwareRevision(sizeof(buffer), buffer);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetHardwareRevision, rc, kSdkError, result);
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

int GobiModem::GetMmActivationState() {
  ULONG device_activation_state;
  ULONG rc;
  rc = sdk_->GetActivationState(&device_activation_state);
  if (rc != 0) {
    LOG(ERROR) << "GetActivationState: " << rc;
    return -1;
  }
  LOG(INFO) << "device activation state: " << device_activation_state;
  if (device_activation_state == 1) {
    return MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
  }

  // Is the modem de-activated, or is there an activation in flight?
  switch (carrier_->activation_method()) {
    case Carrier::kOmadm: {
        ULONG session_state;
        ULONG session_type;
        ULONG failure_reason;
        BYTE retry_count;
        WORD session_pause;
        WORD time_remaining;  // For session pause
        rc = sdk_->OMADMGetSessionInfo(
            &session_state, &session_type, &failure_reason, &retry_count,
            &session_pause, & time_remaining);
        if (rc != 0) {
          // kNoTrackingSessionHasBeenStarted -> modem has never tried
          // to run OMADM; this is not an error condition.
          if (rc != gobi::kNoTrackingSessionHasBeenStarted) {
            LOG(ERROR) << "Could not get omadm state: " << rc;
          }
          return MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
        }
        return (session_state <= gobi::kOmadmMaxFinal) ?
            MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED :
            MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
      }
      break;
    case Carrier::kOtasp:
      return (device_activation_state == gobi::kNotActivated) ?
          MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED :
          MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
      break;
    default:  // This is a UMTS carrier; we count it as activated
      return MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
  }
}

// if we get SDK errors while trying to retrieve information,
// we ignore them, and just don't set the corresponding properties
DBusPropertyMap GobiModem::GetStatus(DBus::Error& error_ignored) {
  DBusPropertyMap result;

  int32_t rssi;
  DBus::Error signal_strength_error;

  StrengthMap interface_to_dbm;
  GetSignalStrengthDbm(rssi, &interface_to_dbm, signal_strength_error);
  if (signal_strength_error.is_set()) {
    // Activate() looks for this key and does not even try if present
    result["no_signal"].writer().append_bool(true);
  } else {
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
    if (carrier_id != carrier_->carrier_id()) {
      LOG(ERROR) << "Inconsistent carrier: id = " << carrier_id;
      std::ostringstream s;
      s << "inconsistent: " << carrier_id << " vs. " << carrier_->carrier_id();
      result["carrier"].writer().append_string(s.str().c_str());
    } else {
      result["carrier"].writer().append_string(carrier_->name());
    }

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

  char mdn[kDefaultBufferSize], min[kDefaultBufferSize];
  rc = sdk_->GetVoiceNumber(kDefaultBufferSize, mdn,
                            kDefaultBufferSize, min);
  if (rc == 0) {
    result["mdn"].writer().append_string(mdn);
    result["min"].writer().append_string(min);
  } else if (rc != gobi::kNotProvisioned) {
    LOG(WARNING) << "GetVoiceNumber failed: " << rc;
  }

  char firmware_revision[kDefaultBufferSize];
  rc = sdk_->GetFirmwareRevision(sizeof(firmware_revision), firmware_revision);
  if (rc == 0 && strlen(firmware_revision) != 0) {
    result["firmware_revision"].writer().append_string(firmware_revision);
  }

  WORD prl_version;
  rc = sdk_->GetPRLVersion(&prl_version);
  if (rc == 0) {
    result["prl_version"].writer().append_uint16(prl_version);
  }

  int activation_state = GetMmActivationState();
  if (activation_state >= 0) {
    result["activation_state"].writer().append_uint32(activation_state);
  }
  carrier_-> ModifyModemStatusReturn(&result);

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
    DLOG(INFO) << "Registration state " << reg_state << " (" << mm_reg_state
               << ") for RFI " << (int)radio_interfaces[i];
    if (radio_interfaces[i] == gobi::kRfiCdma1xRtt)
      cdma_1x_state = mm_reg_state;
    else if (radio_interfaces[i] == gobi::kRfiCdmaEvdo)
      evdo_state = mm_reg_state;
  }
}

// This is only in debug builds; if you add actual code here, see
// RegisterCallbacks().
static void ByteTotalsCallback(ULONGLONG tx, ULONGLONG rx) {
  LOG(INFO) << "ByteTotalsCallback: tx " << tx << " rx " << rx;
}

// This is only in debug builds; if you add actual code here, see
// RegisterCallbacks().
static void DataCapabilitiesCallback(BYTE size, BYTE* caps) {
  ULONG *ucaps = reinterpret_cast<ULONG*>(caps);
  LOG(INFO) << "DataCapabiliesHandler: " << size << " caps";
  for (int i = 0; i < size; i++) {
    LOG(INFO) << "Cap: " << ucaps[i];
  }
}

// This is only in debug builds; if you add actual code here, see
// RegisterCallbacks().
static void DormancyStatusCallback(ULONG status) {
  LOG(INFO) << "DormancyStatusCallback: " << status;
}

static void MobileIPStatusCallback(ULONG status) {
  LOG(INFO) << "MobileIPStatusCallback: " << status;
}

static void PowerCallback(ULONG mode) {
  LOG(INFO) << "PowerCallback: " << mode;
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
  sdk_->SetMobileIPStatusCallback(MobileIPStatusCallback);
  sdk_->SetPowerCallback(PowerCallback);
  sdk_->SetRFInfoCallback(RFInfoCallbackTrampoline);
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

  if (DEBUG) {
    // These are only used for logging.
    sdk_->SetByteTotalsCallback(ByteTotalsCallback, 60);
    sdk_->SetDataCapabilitiesCallback(DataCapabilitiesCallback);
    sdk_->SetDormancyStatusCallback(DormancyStatusCallback);
  }

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
  if (rc != 0) {
    // Can't use assert() - this should happen even in production builds (!)
    LOG(ERROR) << "QCWWANConnect() failed: " << rc;
    abort();
  }

  // The Gobi has been observed (at least once - see chromium-os:7172) to get
  // into a state where we can set up a QMI channel to it (so QCWWANConnect()
  // succeeds) but not actually invoke any functions. We'll force the issue here
  // by calling GetSerialNumbers so we can detect this failure mode early.
  char esn[kDefaultBufferSize];
  char imei[kDefaultBufferSize];
  char meid[kDefaultBufferSize];
  rc = sdk_->GetSerialNumbers(sizeof(esn), esn, sizeof(imei), imei,
                              sizeof(meid), meid);
  if (rc != 0) {
    LOG(ERROR) << "GetSerialNumbers() failed: " << rc;
    abort();
  }

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
    LOG(INFO) << "ESN " << serials.esn
              << " IMEI " << serials.imei
              << " MEID " << serials.meid;
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

void GobiModem::ActivateManual(const DBusPropertyMap& const_properties,
                               DBus::Error &error) {
  using utilities::ExtractString;
  DBusPropertyMap properties(const_properties);

  // TODO(rochberg): Does it make sense to set defaults from the
  // modem's current state?
  const char *spc = NULL;
  uint16_t system_id = 65535;
  const char *mdn = NULL;
  const char *min = NULL;
  const char *mnha = NULL;
  const char *mnaaa = NULL;

  DBusPropertyMap::const_iterator p;
  // try/catch required to cope with dbus-c++'s handling of type
  // mismatches
  try {  // Style guide violation forced by dbus-c++
    spc = ExtractString(properties, "spc", "000000", error);
    p = properties.find("system_id");
    if (p != properties.end()) {
      system_id = p->second.reader().get_uint16();
    }
    mdn = ExtractString(properties, "mdn", "",  error);
    min = ExtractString(properties, "min", "", error);
    mnha = ExtractString(properties, "mnha", NULL, error);
    mnaaa = ExtractString(properties, "mnhaaa", NULL, error);
  } catch (DBus::Error e) {
    error = e;
    return;
  }
  ULONG rc = sdk_->ActivateManual(spc,
                                  system_id,
                                  mdn,
                                  min,
                                  0,          // PRL size
                                  NULL,       // PRL contents
                                  mnha,
                                  mnaaa);
  ENSURE_SDK_SUCCESS(ActivateManual, rc, kActivationError);
}

void GobiModem::ActivateManualDebug(
    const std::map<std::string, std::string>& properties,
    DBus::Error &error) {

  DBusPropertyMap output;

  for (std::map<std::string, std::string>::const_iterator i =
           properties.begin();
       i != properties.end();
       ++i) {
    if (i->first != "system_id") {
      output[i->first].writer().append_string(i->second.c_str());
    } else {
      const std::string& value = i->second;
      char *end;
      errno = 0;
      uint16_t system_id = static_cast<uint16_t>(
          strtoul(value.c_str(), &end, 10));
      if (errno != 0 || *end != '\0') {
        LOG(ERROR) << "Bad system_id: " << value;
        error.set(kSdkError, "Bad system_id");
        return;
      }
      output[i->first].writer().append_uint16(system_id);
    }
  }
  ActivateManual(output, error);
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

uint32_t GobiModem::ActivateOmadm() {
  ULONG rc;
  LOG(INFO) << "Activating OMA-DM";

  rc = sdk_->OMADMStartSession(gobi::kConfigure);
  if (rc != 0) {
    LOG(ERROR) << "OMA-DM activation failed: " << rc;
    return MM_MODEM_CDMA_ACTIVATION_ERROR_START_FAILED;
  }
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
}

uint32_t GobiModem::ActivateOtasp(const std::string& number) {
  ULONG rc;
  LOG(INFO) << "Activating OTASP";

  rc = sdk_->ActivateAutomatic(number.c_str());
  if (rc != 0) {
    LOG(ERROR) << "OTASP activation failed: " << rc;
    return MM_MODEM_CDMA_ACTIVATION_ERROR_START_FAILED;
   }
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
}

void GobiModem::SetCarrier(const std::string& carrier_name,
                           DBus::Error& error) {
  const Carrier *carrier = handler_->server().FindCarrierByName(carrier_name);
  if (carrier == NULL) {
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

  if (modem_carrier_id != carrier->carrier_id()) {

    // UpgradeFirmware doesn't pay attention to anything before the
    // last /, so we don't supply it
    std::string image_path = std::string("/") + carrier->firmware_directory();

    LOG(INFO) << "Current Gobi carrier: " << modem_carrier_id
              << ".  Carrier image selection "
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


gboolean GobiModem::ActivationTimeoutCallback(gpointer data) {
  CallbackArgs* args = static_cast<CallbackArgs*>(data);
  GobiModem* modem = handler_->LookupByPath(*args->path);
  if (modem == NULL)
    return FALSE;

  LOG(ERROR) << "ActivationTimeout";
  modem->SendActivationStateChanged(MM_MODEM_CDMA_ACTIVATION_ERROR_TIMED_OUT);
  return FALSE;
}

void GobiModem::CleanupActivationTimeoutCallback(gpointer data) {
  CallbackArgs *args = static_cast<CallbackArgs*>(data);
  delete args;
}


// NB: This function only uses the DBus::Error field to return
// kOperationInitiatedError.  Other errors are returned as uint32_t
// values from MM_MODEM_CDMA_ACTIVATION_ERROR
uint32_t GobiModem::Activate(const std::string& carrier_name,
                             DBus::Error& activation_started_error) {
  // TODO(ellyjones): This is a guess based on empirical observations; someone
  // must know a real reasonable value for it. Find out what such a value is and
  // insert it here.
  static const int kActivationTimeoutSec = 5;
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

  if (0 != rc) {
    LOG(ERROR) << "GetFirmwareInfo: " << rc;
    return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
  }
  const Carrier *carrier;
  if (carrier_name.empty()) {
    carrier = handler_->server().FindCarrierByCarrierId(carrier_id);
    if (carrier == NULL) {
      LOG(ERROR) << "Unknown carrier id: " << carrier_id;
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
    }
  } else {
    carrier = handler_->server().FindCarrierByName(carrier_name);
    if (carrier == NULL) {
      LOG(WARNING) << "Unknown carrier: " << carrier_name;
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
    }
    if (carrier_id != carrier->carrier_id()) {
      LOG(WARNING) << "Current device firmware does not match the requested"
          "carrier.";
      LOG(WARNING) << "SetCarrier operation must be done before activating.";
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
    }
  }

  DBus::Error internal_error;
  DBusPropertyMap status = GetStatus(internal_error);
  if (internal_error.is_set()) {
    LOG(ERROR) << internal_error;
    return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
  }

  DBusPropertyMap::const_iterator p;
  p = status.find("no_signal");
  if (p != status.end()) {
    return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL;
  }

  p = status.find("activation_state");
  if (p != status.end()) {
    try {  // Style guide violation forced by dbus-c++
      LOG(INFO) << "Current activation state: "
                << p->second.reader().get_uint32();
    } catch (const DBus::Error &e) {
      LOG(ERROR) << e;
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
    }
  }

  activation_start_time_ = time_ms();
  switch (carrier->activation_method()) {
    case Carrier::kOmadm:
      return ActivateOmadm();
      break;

    case Carrier::kOtasp: {
        using org::freedesktop::ModemManager::Modem::Cdma_adaptor;
        Cdma_adaptor *cdma_this = static_cast<Cdma_adaptor *>(this);
        uint32_t immediate_return;
        if (carrier->CdmaCarrierSpecificActivate(
                status, cdma_this, &immediate_return)) {
          return immediate_return;
        }
      }
      if (carrier->activation_code() == NULL) {
        LOG(ERROR) << "Number was not supplied for OTASP activation";
        return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
      }
      return ActivateOtasp(carrier->activation_code());
      break;

    default:
      LOG(ERROR) << "Unknown activation method: "
                   << carrier->activation_method();
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
      break;
  }

  if (activation_callback_id_) {
    g_source_remove(activation_callback_id_);
    activation_callback_id_ = 0;
  }

  activation_callback_id_ = g_timeout_add_seconds_full(
      G_PRIORITY_DEFAULT,
      kActivationTimeoutSec,
      ActivationTimeoutCallback,
      new CallbackArgs(
          new DBus::Path(connected_modem_->path())),
      CleanupActivationTimeoutCallback);
  // We've successfully fired off an activation attempt, so we return
  // the "error" of saying so.
  activation_started_error.set(kOperationInitiatedError, "");
  // Return will be ignored; DBus Error sent insteaad
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
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
    DLOG(INFO) << "Interface " << i << ": " << static_cast<int>(strengths[i])
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

void GobiModem::SendActivationStateFailed() {
  DBusPropertyMap empty;
  ActivationStateChanged(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
                         MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN,
                         empty);
}

void GobiModem::SendActivationStateChanged(uint32_t mm_activation_error) {
  DBusPropertyMap status;
  DBusPropertyMap to_send;
  DBus::Error internal_error;
  status = GetStatus(internal_error);
  if (internal_error.is_set()) {
    // GetStatus should never fail;  we are punting here.
    SendActivationStateFailed();
    return;
  }

  DBusPropertyMap::const_iterator p;
  uint32_t mm_activation_state;

  if ((p = status.find("activation_state")) == status.end()) {
    LOG(ERROR);
    SendActivationStateFailed();
    return;
  }
  try {  // Style guide violation forced by dbus-c++
    mm_activation_state = p->second.reader().get_uint32();
  } catch (const DBus::Error &e) {
    LOG(ERROR);
    SendActivationStateFailed();
  }

  if (mm_activation_error == MM_MODEM_CDMA_ACTIVATION_ERROR_TIMED_OUT &&
      mm_activation_state == MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED) {
    mm_activation_error = MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
  }

  // TODO(rochberg):  Table drive
  if ((p = status.find("mdn")) != status.end()) {
    to_send["mdn"] = p->second;
  }
  if ((p = status.find("min")) != status.end()) {
    to_send["min"] = p->second;
  }
  if ((p = status.find("payment_url")) != status.end()) {
    to_send["payment_url"] = p->second;
  }

  ActivationStateChanged(mm_activation_state,
                         mm_activation_error,
                         to_send);
}



gboolean GobiModem::OmadmStateCallback(gpointer data) {
  OmadmStateArgs* args = static_cast<OmadmStateArgs*>(data);
  LOG(INFO) << "OMA-DM State Callback: " << args->session_state;
  GobiModem* modem = handler_->LookupByPath(*args->path);
  bool activation_done = true;
  if (modem != NULL) {
    switch (args->session_state) {
      case gobi::kOmadmComplete:
        modem->SendActivationStateChanged(
            MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR);
        break;
      case gobi::kOmadmFailed:
        LOG(INFO) << "OMA-DM failure reason: " << args->failure_reason;
        // fall through
      case gobi::kOmadmUpdateInformationUnavailable:
        modem->SendActivationStateChanged(
            MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED);
        break;
      default:
        activation_done = false;
    }
  }

  if (activation_done) {
    ULONG duration = time_ms() - modem->activation_start_time_;
    modem->metrics_lib_->SendToUMA("Network.3G.Gobi.Activation", duration,
                                   0, 150000, 20);
  }

  delete args;
  return FALSE;
}

gboolean GobiModem::ActivationStatusCallback(gpointer data) {
  ActivationStatusArgs* args = static_cast<ActivationStatusArgs*>(data);
  LOG(INFO) << "OTASP status callback: " << args->device_activation_state;
  GobiModem* modem = handler_->LookupByPath(*args->path);

  if (modem != NULL) {
    if (modem->activation_callback_id_) {
      g_source_remove(modem->activation_callback_id_);
      modem->activation_callback_id_ = NULL;
    }
    if (args->device_activation_state == gobi::kActivated ||
        args->device_activation_state == gobi::kNotActivated) {
      ULONG duration = time_ms() - modem->activation_start_time_;
      modem->metrics_lib_->SendToUMA("Network.3G.Gobi.Activation", duration, 0,
                                     150000, 20);
    }
    if (args->device_activation_state == gobi::kActivated) {
      modem->SendActivationStateChanged(
          MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR);
    } else if (args->device_activation_state == gobi::kNotActivated) {
      modem->SendActivationStateChanged(
          MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED);
    }
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
  DLOG(INFO) << "SignalStrengthHandler " << static_cast<int>(signal_strength)
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
  if (state == gobi::kDisconnected) {
    if (disconnect_start_time_) {
      ULONG duration = time_ms() - disconnect_start_time_;
      metrics_lib_->SendToUMA("Network.3G.Gobi.Disconnect", duration, 0, 150000,
                              20);
      disconnect_start_time_ = 0;
    }
    session_id_ = 0;
  } else if (state == gobi::kConnected) {
    ULONG duration = time_ms() - connect_start_time_;
    metrics_lib_->SendToUMA("Network.3G.Gobi.Connect", duration, 0, 150000, 20);
    suspending_ = false;
  }
  bool is_connected = (state == gobi::kConnected);
  unsigned int reason = QMIReasonToMMReason(session_end_reason);
  if (reason == MM_MODEM_CONNECTION_STATE_CHANGE_REASON_REQUESTED && suspending_)
    reason = MM_MODEM_CONNECTION_STATE_CHANGE_REASON_SUSPEND;
  ConnectionStateChanged(is_connected, reason);
}

void GobiModem::RegistrationStateHandler() {
  uint32_t cdma_1x_state;
  uint32_t evdo_state;
  DBus::Error error;

  GetRegistrationState(cdma_1x_state, evdo_state, error);
  if (error.is_set())
    return;
  if (cdma_1x_state != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN ||
      evdo_state != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    ULONG duration = time_ms() - registration_start_time_;
    metrics_lib_->SendToUMA("Network.3G.Gobi.Registration", duration, 0, 150000,
                            20);
  }
  RegistrationStateChanged(cdma_1x_state, evdo_state);
  if (session_state_ == gobi::kConnected) {
    ULONG data_bearer_technology;
    sdk_->GetDataBearerTechnology(&data_bearer_technology);
    // TODO(ers) send a signal or change a property to notify
    // listeners about the change in data bearer technology
  }

  LOG(INFO) << "  => 1xRTT: " << cdma_1x_state << " EVDO: " << evdo_state;
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
          if (grandparent != NULL) {
            sysfs_path_ = udev_device_get_syspath(grandparent);
            LOG(INFO) << "sysfs path: " << sysfs_path_;
            MasterDevice = sysfs_path_;
          }
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
  LOG(INFO) << "StartExit: session id " << session_id_;
  return true;
}

unsigned int GobiModem::QMIReasonToMMReason(unsigned int qmireason) {
  switch (qmireason) {
    case gobi::kClientEndedCall:
      return MM_MODEM_CONNECTION_STATE_CHANGE_REASON_REQUESTED;
    default:
      return MM_MODEM_CONNECTION_STATE_CHANGE_REASON_UNKNOWN;
  }
}

bool GobiModem::StartSuspend() {
  LOG(INFO) << "StartSuspend";
  suspending_ = true;
  if (session_id_)
    sdk_->StopDataSession(session_id_);
  return true;
}

bool StartSuspendTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->StartSuspend();
}

void GobiModem::RegisterStartSuspend(const std::string &name) {
  // TODO(ellyjones): Get maxdelay_ms from the carrier plugin
  static const int maxdelay_ms = 10000;
  handler_->server().RegisterStartSuspend(name, StartSuspendTrampoline, this,
                                          maxdelay_ms);
}
