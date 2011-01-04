// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef PLUGIN_GOBI_MODEM_H_
#define PLUGIN_GOBI_MODEM_H_

#include <glib.h>
#include <pthread.h>
#include <base/basictypes.h>
// TODO(ers) remove following #include once logging spew is resolved
#include <glog/logging.h>
#include <gtest/gtest_prod.h>  // For FRIEND_TEST
#include <map>
#include <string>

#include <cromo/cromo_server.h>
#include <cromo/modem_server_glue.h>
#include <cromo/modem-simple_server_glue.h>
#include <cromo/utilities.h>
#include <metrics/metrics_library.h>

#include "modem_gobi_server_glue.h"
#include "gobi_sdk_wrapper.h"


// TODO(rochberg)  Fix namespace pollution
#define METRIC_BASE_NAME "Network.3G.Gobi."

#define DEFINE_ERROR(name) \
  extern const char* k##name##Error;
#define DEFINE_MM_ERROR(name) \
  extern const char* k##name##Error;
#include "gobi_modem_errors.h"
#undef DEFINE_ERROR
#undef DEFINE_MM_ERROR

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


// Qualcomm device element, capitalized to their naming conventions
struct DEVICE_ELEMENT {
  char deviceNode[256];
  char deviceKey[16];
};

class Carrier;
class CromoServer;
class GobiModemHandler;

class GobiModem
    : public org::freedesktop::ModemManager::Modem_adaptor,
      public org::freedesktop::ModemManager::Modem::Simple_adaptor,

      public org::chromium::ModemManager::Modem::Gobi_adaptor,
      public DBus::IntrospectableAdaptor,
      public DBus::PropertiesAdaptor,
      public DBus::ObjectAdaptor {
 public:
  typedef std::map<ULONG, int> StrengthMap;

  GobiModem(DBus::Connection& connection,
            const DBus::Path& path,
            const DEVICE_ELEMENT &device,
            gobi::Sdk *sdk);
  virtual ~GobiModem();

  virtual void Init();

  int last_seen() {return last_seen_;}
  void set_last_seen(int scan_count) {
    last_seen_ = scan_count;
  }

  static void set_handler(GobiModemHandler* handler) { handler_ = handler; }

  // Called by the SDK wrapper when it receives an error that requires
  // that the modem be reset.  Posts a main-thread callback that
  // power cycles the modem.
  static void SinkSdkError(const std::string& modem_path,
                           const std::string& sdk_function,
                           ULONG error);

  std::string GetUSBAddress();

  // DBUS Methods: Modem
  virtual void Enable(const bool& enable, DBus::Error& error);
  virtual void Connect(const std::string& number, DBus::Error& error);
  virtual void Disconnect(DBus::Error& error);
  virtual void FactoryReset(const std::string& number, DBus::Error& error);

  virtual ::DBus::Struct<
  uint32_t, uint32_t, uint32_t, uint32_t> GetIP4Config(DBus::Error& error);

  virtual ::DBus::Struct<
    std::string, std::string, std::string> GetInfo(DBus::Error& error);

  // DBUS Methods: ModemSimple
  virtual void Connect(const utilities::DBusPropertyMap& properties,
                       DBus::Error& error);

  // Contract addition: GetStatus never fails, it simply does not set
  // properties it cannot determine.
  virtual utilities::DBusPropertyMap GetStatus(DBus::Error& error);

  // DBUS Methods: ModemGobi
  virtual void SetCarrier(const std::string& image, DBus::Error& error);
  virtual void SoftReset(DBus::Error& error);
  virtual void PowerCycle(DBus::Error& error);
  virtual void RequestEvents(const std::string& events, DBus::Error& error);

 protected:
  struct SerialNumbers {
    std::string esn;
    std::string imei;
    std::string meid;
  };

  void ApiConnect(DBus::Error& error);
  ULONG ApiDisconnect(void);
  virtual uint32_t CommonGetSignalQuality(DBus::Error& error);
  void GetSignalStrengthDbm(int& strength,
                            StrengthMap *interface_to_strength,
                            DBus::Error& error);

  virtual void RegisterCallbacks();
  void ResetModem(DBus::Error& error);

  void GetSerialNumbers(SerialNumbers* out, DBus::Error &error);
  void LogGobiInformation();

  // TODO(cros-connectivity):  Move these to a utility class
  static unsigned long MapDbmToPercent(INT8 signal_strength_dbm);
  static unsigned long MapDataBearerToRfi(ULONG data_bearer_technology);
  static unsigned long long GetTimeMs(void);

  struct CallbackArgs {
    CallbackArgs() : path(NULL) { }
    explicit CallbackArgs(DBus::Path *path) : path(path) { }
    ~CallbackArgs() { delete path; }
    DBus::Path* path;
   private:
    DISALLOW_COPY_AND_ASSIGN(CallbackArgs);
  };

  static void PostCallbackRequest(GSourceFunc callback,
                                  CallbackArgs* args) {
    pthread_mutex_lock(&modem_mutex_.mutex_);
    if (connected_modem_) {
      args->path = new DBus::Path(connected_modem_->path());
      g_idle_add(callback, args);
    } else {
      delete args;
    }
    pthread_mutex_unlock(&modem_mutex_.mutex_);
  }

  struct NmeaPlusArgs : public CallbackArgs {
    NmeaPlusArgs(LPCSTR nmea, ULONG mode)
      : nmea(nmea),
        mode(mode) { }
    std::string nmea;
    ULONG mode;
  };

  static void NmeaPlusCallbackTrampoline(LPCSTR nmea, ULONG mode) {
    PostCallbackRequest(NmeaPlusCallback,
                        new NmeaPlusArgs(nmea, mode));
  }
  static gboolean NmeaPlusCallback(gpointer data);

  struct SessionStateArgs : public CallbackArgs {
    SessionStateArgs(ULONG state,
                     ULONG session_end_reason)
      : state(state),
        session_end_reason(session_end_reason) { }
    ULONG state;
    ULONG session_end_reason;
  };

  static void SessionStateCallbackTrampoline(ULONG state,
                                             ULONG session_end_reason) {
    PostCallbackRequest(SessionStateCallback,
                        new SessionStateArgs(state,
                                             session_end_reason));
  }
  static gboolean SessionStateCallback(gpointer data);

  static void DataBearerCallbackTrampoline(ULONG data_bearer_technology) {
    // ignore the supplied argument
    PostCallbackRequest(RegistrationStateCallback, new CallbackArgs());
  }

  static void RoamingIndicatorCallbackTrampoline(ULONG roaming) {
    // ignore the supplied argument
    PostCallbackRequest(RegistrationStateCallback, new CallbackArgs());
  }

  static void RFInfoCallbackTrampoline(ULONG iface, ULONG bandclass,
                                       ULONG channel) {
    // ignore the supplied arguments
    PostCallbackRequest(RegistrationStateCallback, new CallbackArgs());
  }
  static gboolean RegistrationStateCallback(gpointer data);

  struct SignalStrengthArgs : public CallbackArgs {
    SignalStrengthArgs(INT8 signal_strength,
                       ULONG radio_interface)
      : signal_strength(signal_strength),
        radio_interface(radio_interface) { }
    INT8 signal_strength;
    ULONG radio_interface;
  };

  static void SignalStrengthCallbackTrampoline(INT8 signal_strength,
                                               ULONG radio_interface) {
    PostCallbackRequest(SignalStrengthCallback,
                        new SignalStrengthArgs(signal_strength,
                                               radio_interface));
  }
  static gboolean SignalStrengthCallback(gpointer data);

  struct DormancyStatusArgs : public CallbackArgs {
    DormancyStatusArgs(ULONG status) : status(status) { }
    ULONG status;
  };

  static void DormancyStatusCallbackTrampoline(ULONG status) {
    PostCallbackRequest(DormancyStatusCallback,
                        new DormancyStatusArgs(status));
  }

  static gboolean DormancyStatusCallback(gpointer data);

  struct SdkErrorArgs : public CallbackArgs {
    SdkErrorArgs(ULONG error) : error(error) { }
    ULONG error;
  };
  static gboolean SdkErrorHandler(gpointer data);

  static void *NMEAThreadTrampoline(void *arg) {
    if (connected_modem_)
      return connected_modem_->NMEAThread();
    else
      return NULL;
  }
  void *NMEAThread(void);


  static unsigned int QMIReasonToMMReason(unsigned int qmireason);
  // Set DBus-exported properties
  void SetDeviceProperties();
  void SetModemProperties();
  virtual void SetDeviceSpecificIdentifier(const SerialNumbers&);

  // Returns the modem activation state as an enum value from
  // MM_MODEM_CDMA_ACTIVATION_STATE_..., or < 0 for error.  This state
  // may be further overridden by ModifyModemStatusReturn()
  int32_t GetMmActivationState();

  void GetGobiRegistrationState(ULONG* cdma_1x_state, ULONG* cdma_evdo_state,
                                ULONG* roaming_state, DBus::Error& error);

  void StartNMEAThread();
  // Handlers for events delivered as callbacks by the SDK. These
  // all run in the main thread.
  void RegistrationStateHandler();

  // Overridden in CDMA, GSM
  virtual void SignalStrengthHandler(INT8 signal_strength,
                                     ULONG radio_interface);
  void SessionStateHandler(ULONG state, ULONG session_end_reason);

  static GobiModemHandler *handler_;
  // Wraps the Gobi SDK for dependency injection
  gobi::Sdk *sdk_;
  DEVICE_ELEMENT device_;
  int last_seen_;  // Updated every scan where the modem is present
  int nmea_fd_; // fifo to write NMEA data to

  pthread_t nmea_thread;

  ULONG session_state_;
  ULONG session_id_;

  std::string sysfs_path_;

  struct mutex_wrapper_ {
    mutex_wrapper_() { pthread_mutex_init(&mutex_, NULL); }
    pthread_mutex_t mutex_;
  };
  static GobiModem *connected_modem_;
  static mutex_wrapper_ modem_mutex_;

  bool suspending_;
  bool exiting_;
  bool device_resetting_;
  const Carrier *carrier_;

  friend class GobiModemTest;

  bool is_disconnected() { return session_id_ == 0; }

  bool StartExit();
  bool ExitOk();
  bool StartSuspend();
  bool SuspendOk();
  void RegisterStartSuspend(const std::string& name);

  // Resets the device by kicking it off the USB and allowing it back
  // on.  Reason is a QCWWAN error code for logging/metrics
  void ResetDevice(ULONG reason);
  // Helper that compresses reason to a small int and mails to UMA
  void RecordResetReason(ULONG reason);

  bool CanMakeMethodCalls(void);

  std::string hooks_name_;

  friend bool StartExitTrampoline(void *arg);
  friend bool ExitOkTrampoline(void *arg);
  friend bool StartSuspendTrampoline(void *arg);
  friend bool SuspendOkTrampoline(void *arg);

  scoped_ptr<MetricsLibraryInterface> metrics_lib_;


  unsigned long long connect_start_time_;
  unsigned long long disconnect_start_time_;
  unsigned long long registration_start_time_;

  // This ought to go on the gobi modem XML interface, but dbus-c++ can't handle
  // enums, and all the ways of hacking around it seem worse than just using
  // strings and translating to the enum when we get the DBus call.
  enum {
    GOBI_EVENT_DORMANCY = 0,
    GOBI_EVENT_MAX
  };

  static int EventKeyToIndex(const char *key);
  void RequestEvent(const std::string req, DBus::Error& error);
  bool event_enabled[GOBI_EVENT_MAX];

  FRIEND_TEST(GobiModemTest, GetSignalStrengthDbmDisconnected);
  FRIEND_TEST(GobiModemTest, GetSignalStrengthDbmConnected);

  DISALLOW_COPY_AND_ASSIGN(GobiModem);
};
#endif  // PLUGIN_GOBI_MODEM_H_
