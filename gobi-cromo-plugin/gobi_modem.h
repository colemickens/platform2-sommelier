// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef PLUGIN_GOBI_MODEM_H_
#define PLUGIN_GOBI_MODEM_H_

#include <pthread.h>
#include <base/basictypes.h>
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
      public DBus::ObjectAdaptor,
      public DBus::PropertiesAdaptor {
 public:
  GobiModem(DBus::Connection& connection,
            const DBus::Path& path,
            GobiModemHandler *handler,
            const DEVICE_ELEMENT &device,
            gobi::Sdk *sdk);

  virtual ~GobiModem() {}

  int last_seen() {return last_seen_;}
  void set_last_seen(int scan_count) {
    last_seen_ = scan_count;
  }

  // DBUS Methods: Modem
  virtual void Enable(const bool& enable);
  virtual void Connect(const std::string& number);
  virtual void Disconnect();
  virtual void FactoryReset(const std::string& number);

  virtual ::DBus::Struct<
  uint32_t, uint32_t, uint32_t, uint32_t> GetIP4Config();

  virtual ::DBus::Struct<
    std::string, std::string, std::string> GetInfo();

  // DBUS Methods: ModemSimple
  virtual void Connect(const DBusPropertyMap& properties);
  virtual DBusPropertyMap GetStatus();

  // DBUS Methods: ModemCDMA
  virtual uint32_t GetSignalQuality();
  virtual std::string GetEsn();
  virtual DBus::Struct<uint32_t, std::string, uint32_t> GetServingSystem();
  virtual void GetRegistrationState(
      uint32_t& cdma_1x_state, uint32_t& evdo_state);

  // DBUS Methods: ModemGobi
  virtual void SetCarrier(const std::string& image);
  virtual void SoftReset();

  // DBUS Property Getter
  virtual void on_get_property(DBus::InterfaceAdaptor& interface,
                               const std::string& property,
                               DBus::Variant& vale);

 protected:
  bool ActivateOmadm();
  bool ActivateOtasp();  // Verizon uses OTASP
  bool ApiConnect();
  bool EnsureActivated();
  bool EnsureFirmwareLoaded(const char *carrier_name);
  bool GetSignalStrengthDbm(int *strength);
  bool ResetModem();

  struct SerialNumbers {
    std::string esn;
    std::string imei;
    std::string meid;
  };
  bool GetSerialNumbers(SerialNumbers *out);
  void LogGobiInformation();

  static void ActivationStatusTrampoline(ULONG activation_status) {
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

  static void OmadmStateCallbackTrampoline(ULONG sessionState,
                                           ULONG failureReason) {
    if (connected_modem_) {
      connected_modem_->OmadmStateCallback(sessionState, failureReason);
    }
  }
  void OmadmStateCallback(ULONG sessionState, ULONG failureReason);


 private:

  GobiModemHandler *handler_;
  // Wraps the Gobi SDK for dependency injection
  gobi::Sdk *sdk_;
  DEVICE_ELEMENT device_;
  int last_seen_;  // Updated every scan where the modem is present

  // TODO(rochberg):  Do we want these static?
  pthread_mutex_t activation_mutex_;
  pthread_cond_t activation_cond_;
  ULONG activation_state_;

  static GobiModem *connected_modem_;

  DISALLOW_COPY_AND_ASSIGN(GobiModem);
};

#endif  // PLUGIN_GOBI_MODEM_H_
