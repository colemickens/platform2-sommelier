// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_DUMMY_MODEM_H_
#define CROMO_DUMMY_MODEM_H_

#include "base/basictypes.h"

#include <dbus-c++/dbus.h>

#include "modem-cdma_server_glue.h"
#include "modem_server_glue.h"
#include "modem-simple_server_glue.h"

typedef std::map<std::string, DBus::Variant> PropertyMap;

class DummyModem
    : public org::freedesktop::ModemManager::Modem_adaptor,
      public org::freedesktop::ModemManager::Modem::Simple_adaptor,
      public org::freedesktop::ModemManager::Modem::Cdma_adaptor,
      public DBus::IntrospectableAdaptor,
      public DBus::ObjectAdaptor {
 public:
  DummyModem(DBus::Connection& connection, const DBus::Path& path);

  // DBUS Methods: Modem
  void Enable(const bool& enable, DBus::Error& error);
  void Connect(const std::string& number, DBus::Error& error);
  void Disconnect(DBus::Error& error);
  void FactoryReset(const std::string& code, DBus::Error& error);
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> GetIP4Config(
      DBus::Error& error);
  DBus::Struct<std::string, std::string, std::string> GetInfo(
      DBus::Error& error);

  // DBUS Methods: ModemSimple
  void Connect(const PropertyMap& properties, DBus::Error& error);
  PropertyMap GetStatus(DBus::Error& error);

  // DBUS Methods: ModemCdma
  uint32_t GetSignalQuality(DBus::Error& error);
  std::string GetEsn(DBus::Error& error);
  DBus::Struct<uint32_t, std::string, uint32_t> GetServingSystem(
      DBus::Error& error);
  void GetRegistrationState(uint32_t& cdma_1x_state, uint32_t& evdo_state,
                            DBus::Error& error);
  void Activate(const std::string &carrier, DBus::Error &error);

  DISALLOW_COPY_AND_ASSIGN(DummyModem);
};

#endif // CROMO_DUMMY_MODEM_H_
