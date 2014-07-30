// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo/dummy_modem.h"

#include <mm/mm-modem.h>

#include <base/logging.h>

using std::string;

DummyModem::DummyModem(DBus::Connection& connection,  // NOLINT - refs.
                       const DBus::Path& path)
    : DBus::ObjectAdaptor(connection, path) {}

// DBUS Methods: Modem
void DummyModem::Enable(const bool& enable,
                        DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "Enable: " << enable;
}

void DummyModem::Connect(const string& number,
                         DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "Connect: " << number;
}

void DummyModem::Disconnect(DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "Disconnect";
}

void DummyModem::FactoryReset(const string& code,
                              DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "FactoryReset: " << code;
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> DummyModem::GetIP4Config(
    DBus::Error& error) {  // NOLINT - refs.
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  LOG(INFO) << "GetIP4Config";

  return result;
}

DBus::Struct<string, string, string> DummyModem::GetInfo(
    DBus::Error& error) {  // NOLINT - refs.
  DBus::Struct<string, string, string> result;
  LOG(INFO) << "GetInfo";
  return result;
}

void DummyModem::Reset(DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "Reset";
}

// DBUS Methods: ModemSimple
void DummyModem::Connect(const PropertyMap& properties,
                         DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "Simple.Connect";
}

PropertyMap DummyModem::GetStatus(DBus::Error& error) {  // NOLINT - refs.
  PropertyMap result;

  LOG(INFO) << "GetStatus";

  return result;
}

// DBUS Methods: ModemCDMA
uint32_t DummyModem::GetSignalQuality(DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "GetSignalQuality";

  return 50;
}

string DummyModem::GetEsn(DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "GetEsn";

  return "12345";
}

DBus::Struct<uint32_t, string, uint32_t> DummyModem::GetServingSystem(
    DBus::Error& error) {  // NOLINT - refs.
  DBus::Struct<uint32_t, string, uint32_t> result;

  LOG(INFO) << "GetServingSystem";

  return result;
}

void DummyModem::GetRegistrationState(uint32_t& cdma_1x_state,  // NOLINT - refs
                                      uint32_t& evdo_state,  // NOLINT - refs.
                                      DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "GetRegistrationState";
}

uint32_t DummyModem::Activate(const std::string& carrier,
                              DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "Activate";
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
}

void DummyModem::ActivateManual(const PropertyMap& properties,
                                DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "ActivateManual";
}

void DummyModem::ActivateManualDebug(
    const std::map<std::string, std::string>& properties,
    DBus::Error& error) {  // NOLINT - refs.
  LOG(INFO) << "ActivateManualDebug";
}
