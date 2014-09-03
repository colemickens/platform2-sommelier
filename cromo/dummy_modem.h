// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_DUMMY_MODEM_H_
#define CROMO_DUMMY_MODEM_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "dbus_adaptors/org.freedesktop.ModemManager.Modem.h"
#include "dbus_adaptors/org.freedesktop.ModemManager.Modem.Cdma.h"  // NOLINT
#include "dbus_adaptors/org.freedesktop.ModemManager.Modem.Simple.h"

typedef std::map<std::string, DBus::Variant> PropertyMap;

class DummyModem : public org::freedesktop::ModemManager::Modem_adaptor,
                   public org::freedesktop::ModemManager::Modem::Simple_adaptor,
                   public org::freedesktop::ModemManager::Modem::Cdma_adaptor,
                   public DBus::IntrospectableAdaptor,
                   public DBus::ObjectAdaptor {
 public:
  DummyModem(DBus::Connection& connection,  // NOLINT - refs.
             const DBus::Path& path);

  // DBUS Methods: Modem
  void Enable(const bool& enable, DBus::Error& error);  // NOLINT - refs.
  void Connect(const std::string& number,
               DBus::Error& error);  // NOLINT - refs.
  void Disconnect(DBus::Error& error);  // NOLINT - refs.
  void FactoryReset(const std::string& code,
                    DBus::Error& error);  // NOLINT - refs.
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> GetIP4Config(
      DBus::Error& error);  // NOLINT - refs.
  DBus::Struct<std::string, std::string, std::string> GetInfo(
      DBus::Error& error);  // NOLINT - refs.
  void Reset(DBus::Error& error);  // NOLINT - refs.

  // DBUS Methods: ModemSimple
  void Connect(const PropertyMap& properties,
               DBus::Error& error);  // NOLINT - refs.
  PropertyMap GetStatus(DBus::Error& error);  // NOLINT - refs.

  // DBUS Methods: ModemCdma
  uint32_t GetSignalQuality(DBus::Error& error);  // NOLINT - refs.
  std::string GetEsn(DBus::Error& error);  // NOLINT - refs.
  DBus::Struct<uint32_t, std::string, uint32_t> GetServingSystem(
      DBus::Error& error);  // NOLINT - refs.
  void GetRegistrationState(uint32_t& cdma_1x_state,  // NOLINT - refs.
                            uint32_t& evdo_state,  // NOLINT - refs.
                            DBus::Error& error);  // NOLINT - refs.
  uint32_t Activate(const std::string& carrier,
                    DBus::Error& error);  // NOLINT - refs.
  void ActivateManual(const PropertyMap& properties,
                      DBus::Error& error);  // NOLINT - refs.
  void ActivateManualDebug(const std::map<std::string, std::string>& properties,
                           DBus::Error& error);  // NOLINT - refs.

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyModem);
};

#endif  // CROMO_DUMMY_MODEM_H_
