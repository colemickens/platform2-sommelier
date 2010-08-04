// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef PLUGIN_GOBI_MODEM_H_
#define PLUGIN_GOBI_MODEM_H_

#include <pthread.h>
#include <base/basictypes.h>
#include <gtest/gtest_prod.h>  // For FRIEND_TEST
#include <map>

#include <cromo/modem_server_glue.h>
#include <cromo/modem-simple_server_glue.h>
#include <cromo/modem-cdma_server_glue.h>

#include "modem_gobi_server_glue.h"
#include "gobi_sdk_wrapper.h"


typedef std::map<std::string, DBus::Variant> DBusPropertyMap;

// Qualcomm device element, capitalized to their naming conventions
struct DEVICE_ELEMENT {
  char deviceNode[256];
  char deviceKey[16];
};

class GobiModemHandler;
class GobiModem
    : public org::freedesktop::ModemManager::Modem_adaptor,
      public org::freedesktop::ModemManager::Modem::Simple_adaptor,
      public org::freedesktop::ModemManager::Modem::Cdma_adaptor,
      public org::chromium::ModemManager::Modem::Gobi_adaptor,
      public DBus::IntrospectableAdaptor,
      public DBus::PropertiesAdaptor,
      public DBus::ObjectAdaptor {
 public:
  typedef std::map<ULONG, int> StrengthMap;

  GobiModem(DBus::Connection& connection,
            const DBus::Path& path,
            GobiModemHandler *handler,
            const DEVICE_ELEMENT &device,
            gobi::Sdk *sdk);

  virtual ~GobiModem();

  int last_seen() {return last_seen_;}
  void set_last_seen(int scan_count) {
    last_seen_ = scan_count;
  }

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
  virtual void Connect(const DBusPropertyMap& properties, DBus::Error& error);
  virtual DBusPropertyMap GetStatus(DBus::Error& error);

  // DBUS Methods: ModemCDMA
  virtual uint32_t GetSignalQuality(DBus::Error& error);
  virtual std::string GetEsn(DBus::Error& error);
  virtual DBus::Struct<uint32_t, std::string, uint32_t> GetServingSystem(
      DBus::Error& error);
  virtual void GetRegistrationState(
      uint32_t& cdma_1x_state, uint32_t& evdo_state, DBus::Error& error);

  // DBUS Methods: ModemGobi
  virtual void SetCarrier(const std::string& image, DBus::Error& error);
  virtual void SoftReset(DBus::Error& error);
  virtual void Activate(const std::string& carrier_name,
                        DBus::Error& error);
  virtual void PowerCycle(DBus::Error& error);

  // DBUS Property Getter
  virtual void on_get_property(DBus::InterfaceAdaptor& interface,
                               const std::string& property,
                               DBus::Variant& value,
                               DBus::Error& error);

  void UpdateDataBearerTechnology();
  void UpdateRegistrationState();

 protected:
  void ActivateOmadm(DBus::Error& error);
  // Verizon uses OTASP, code *22899
  void ActivateOtasp(const std::string& number, DBus::Error& error);
  void ApiConnect(DBus::Error& error);
  void EnsureFirmwareLoaded(const char* carrier_name, DBus::Error& error);
  void GetSignalStrengthDbm(int& strength,
                            StrengthMap *interface_to_strength,
                            DBus::Error& error);
  void RegisterCallbacks();
  void ResetModem(DBus::Error& error);

  struct SerialNumbers {
    std::string esn;
    std::string imei;
    std::string meid;
  };
  void GetSerialNumbers(SerialNumbers* out, DBus::Error &error);
  void LogGobiInformation();

  static void ActivationStatusCallbackTrampoline(ULONG activation_status) {
    if (connected_modem_) {
      connected_modem_->ActivationStatusCallback(activation_status);
    }
  }
  void ActivationStatusCallback(ULONG activation_status);

  static void NmeaPlusCallbackTrampoline(LPCSTR nmea, ULONG mode) {
    if (connected_modem_) {
      connected_modem_->NmeaPlusCallback(nmea, mode);
    }
  }
  void NmeaPlusCallback(const char *nmea, ULONG mode);

  static void OmadmStateCallbackTrampoline(ULONG session_state,
                                           ULONG failure_reason) {
    if (connected_modem_) {
      connected_modem_->OmadmStateCallback(session_state, failure_reason);
    }
  }
  void OmadmStateCallback(ULONG session_state, ULONG failure_reason);

  static void SessionStateCallbackTrampoline(ULONG state,
                                             ULONG session_end_reason) {
    if (connected_modem_) {
      connected_modem_->SessionStateCallback(state, session_end_reason);
    }
  }
  void SessionStateCallback(ULONG state, ULONG session_end_reason);

  static void DataBearerCallbackTrampoline(ULONG data_bearer_technology) {
    if (connected_modem_) {
      connected_modem_->DataBearerCallback(data_bearer_technology);
    }
  }
  void DataBearerCallback(ULONG dataBearerTechnology);

  static void RoamingIndicatorCallbackTrampoline(ULONG roaming) {
    if (connected_modem_) {
      connected_modem_->RoamingIndicatorCallback(roaming);
    }
  }
  void RoamingIndicatorCallback(ULONG roaming);

  static void SignalStrengthCallbackTrampoline(INT8 signal_strength,
                                               ULONG radio_interface) {
    if (connected_modem_) {
      connected_modem_->SignalStrengthCallback(signal_strength, radio_interface);
    }
  }
  void SignalStrengthCallback(INT8 signal_strength, ULONG radio_interface);

  static void *NMEAThreadTrampoline(void *arg) {
    if (connected_modem_)
      return connected_modem_->NMEAThread();
    else
      return NULL;
  }
  void *NMEAThread(void);

 private:
  void SetDeviceProperties();
  void SetModemProperties();

  void StartNMEAThread();

  GobiModemHandler *handler_;
  // Wraps the Gobi SDK for dependency injection
  gobi::Sdk *sdk_;
  DEVICE_ELEMENT device_;
  int last_seen_;  // Updated every scan where the modem is present
  int nmea_fd; // fifo to write NMEA data to

  pthread_t nmea_thread;

  pthread_mutex_t activation_mutex_;
  pthread_cond_t activation_cond_;
  ULONG activation_state_;
  ULONG session_state_;
  ULONG session_id_;
  INT8  signal_strength_;

  static GobiModem *connected_modem_;

  friend class GobiModemTest;
  FRIEND_TEST(GobiModemTest, GetSignalStrengthDbmDisconnected);

  DISALLOW_COPY_AND_ASSIGN(GobiModem);
};

#endif  // PLUGIN_GOBI_MODEM_H_
