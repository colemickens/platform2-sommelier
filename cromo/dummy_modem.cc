// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dummy_modem.h"
#include <mm/mm-modem.h>
#include <iostream>

using std::cout;
using std::endl;
using std::string;

DummyModem::DummyModem(DBus::Connection& connection,
                       const DBus::Path& path)
    : DBus::ObjectAdaptor(connection, path) {
}

// DBUS Methods: Modem
void DummyModem::Enable(const bool& enable, DBus::Error& error) {
  cout << "Enable: " << enable << endl;
}

void DummyModem::Connect(const string& number, DBus::Error& error) {
  cout << "Connect: " << number << endl;
}

void DummyModem::Disconnect(DBus::Error& error) {
  cout << "Disconnect" << endl;
}

void DummyModem::FactoryReset(const string& code, DBus::Error& error) {
  cout << "FactoryReset: " << code << endl;
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> DummyModem::GetIP4Config(
    DBus::Error& error) {
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  cout << "GetIP4Config" << endl;

  return result;
}

DBus::Struct<string, string, string> DummyModem::GetInfo(DBus::Error& error) {
  DBus::Struct<string, string, string> result;
  cout << "GetInfo" << endl;
  return result;
}

// DBUS Methods: ModemSimple
void DummyModem::Connect(const PropertyMap& properties, DBus::Error& error) {
  cout << "Simple.Connect" << endl;
}

PropertyMap DummyModem::GetStatus(DBus::Error& error) {
  PropertyMap result;

  cout << "GetStatus" << endl;

  return result;
}

  // DBUS Methods: ModemCDMA
uint32_t DummyModem::GetSignalQuality(DBus::Error& error) {
  cout << "GetSignalQuality" << endl;

  return 50;
}

string DummyModem::GetEsn(DBus::Error& error) {
  cout << "GetEsn" << endl;

  return "12345";
}

DBus::Struct<uint32_t, string, uint32_t> DummyModem::GetServingSystem(
    DBus::Error& error) {
  DBus::Struct<uint32_t, string, uint32_t> result;

  cout << "GetServingSystem" << endl;

  return result;
}

void DummyModem::GetRegistrationState(uint32_t& cdma_1x_state,
                                      uint32_t& evdo_state,
                                      DBus::Error& error
                                      ) {
  cout << "GetRegistrationState" << endl;
}

uint32_t DummyModem::Activate(const std::string& carrier, DBus::Error &error) {
  cout << "Activate" << endl;
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
}

void DummyModem::ActivateManual(const PropertyMap& properties,
                                DBus::Error &error) {
  cout << "ActivateManual" << endl;
}

void DummyModem::ActivateManualDebug(
    const std::map<std::string, std::string> &properties,
    DBus::Error &error) {
  cout << "ActivateManualDebug" << endl;
}
