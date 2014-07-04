// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo/dummy_modem.h"

#include <mm/mm-modem.h>

#include <base/logging.h>

using std::string;

DummyModem::DummyModem(DBus::Connection& connection, const DBus::Path& path)
    : DBus::ObjectAdaptor(connection, path) {}

// DBUS Methods: Modem
void DummyModem::Enable(const bool& enable, DBus::Error& error) {
  LOG(INFO) << "Enable: " << enable;
}

void DummyModem::Connect(const string& number, DBus::Error& error) {
  LOG(INFO) << "Connect: " << number;
}

void DummyModem::Disconnect(DBus::Error& error) {
  LOG(INFO) << "Disconnect";
}

void DummyModem::FactoryReset(const string& code, DBus::Error& error) {
  LOG(INFO) << "FactoryReset: " << code;
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> DummyModem::GetIP4Config(
    DBus::Error& error) {
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  LOG(INFO) << "GetIP4Config";

  return result;
}

DBus::Struct<string, string, string> DummyModem::GetInfo(DBus::Error& error) {
  DBus::Struct<string, string, string> result;
  LOG(INFO) << "GetInfo";
  return result;
}

void DummyModem::Reset(DBus::Error& error) {
  LOG(INFO) << "Reset";
}

// DBUS Methods: ModemSimple
void DummyModem::Connect(const PropertyMap& properties, DBus::Error& error) {
  LOG(INFO) << "Simple.Connect";
}

PropertyMap DummyModem::GetStatus(DBus::Error& error) {
  PropertyMap result;

  LOG(INFO) << "GetStatus";

  return result;
}

// DBUS Methods: ModemCDMA
uint32_t DummyModem::GetSignalQuality(DBus::Error& error) {
  LOG(INFO) << "GetSignalQuality";

  return 50;
}

string DummyModem::GetEsn(DBus::Error& error) {
  LOG(INFO) << "GetEsn";

  return "12345";
}

DBus::Struct<uint32_t, string, uint32_t> DummyModem::GetServingSystem(
    DBus::Error& error) {
  DBus::Struct<uint32_t, string, uint32_t> result;

  LOG(INFO) << "GetServingSystem";

  return result;
}

void DummyModem::GetRegistrationState(uint32_t& cdma_1x_state,
                                      uint32_t& evdo_state,
                                      DBus::Error& error) {
  LOG(INFO) << "GetRegistrationState";
}

uint32_t DummyModem::Activate(const std::string& carrier, DBus::Error& error) {
  LOG(INFO) << "Activate";
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
}

void DummyModem::ActivateManual(const PropertyMap& properties,
                                DBus::Error& error) {
  LOG(INFO) << "ActivateManual";
}

void DummyModem::ActivateManualDebug(
    const std::map<std::string, std::string>& properties, DBus::Error& error) {
  LOG(INFO) << "ActivateManualDebug";
}
