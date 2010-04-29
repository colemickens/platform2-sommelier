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
  void Enable(const bool& enable);
  void Connect(const std::string& number);
  void Disconnect();
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> GetIP4Config();
  DBus::Struct<std::string, std::string, std::string> GetInfo();

  // DBUS Methods: ModemSimple
  void Connect(const PropertyMap& properties);
  PropertyMap GetStatus();

  // DBUS Methods: ModemCdma
  uint32_t GetSignalQuality();
  std::string GetEsn();
  DBus::Struct<uint32_t, std::string, uint32_t> GetServingSystem();
  void GetRegistrationState(uint32_t& cdma_1x_state, uint32_t& evdo_state);

  DISALLOW_COPY_AND_ASSIGN(DummyModem);
};

#endif // CROMO_DUMMY_MODEM_H_
