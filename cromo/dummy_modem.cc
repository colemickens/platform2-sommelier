// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dummy_modem.h"

#include <iostream>

using std::cout;
using std::endl;
using std::string;

DummyModem::DummyModem(DBus::Connection& connection,
                       const DBus::Path& path)
    : DBus::ObjectAdaptor(connection, path) {
}

// DBUS Methods: Modem
void DummyModem::Enable(const bool& enable) {
  cout << "Enable: " << enable << endl;
}

void DummyModem::Connect(const string& number) {
  cout << "Connect: " << number << endl;
}

void DummyModem::Disconnect() {
  cout << "Disconnect" << endl;
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> DummyModem::GetIP4Config() {
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  cout << "GetIP4Config" << endl;

  return result;
}

DBus::Struct<string, string, string> DummyModem::GetInfo() {
  DBus::Struct<string, string, string> result;
  cout << "Disconnect" << endl;
  return result;
}

// DBUS Methods: ModemSimple
void DummyModem::Connect(const PropertyMap& properties) {
  cout << "Simple.Connect" << endl;
}

PropertyMap DummyModem::GetStatus() {
  PropertyMap result;

  cout << "GetStatus" << endl;

  return result;
}

  // DBUS Methods: ModemCDMA
uint32_t DummyModem::GetSignalQuality() {
  cout << "GetSignalQuality" << endl;

  return 50;
}

string DummyModem::GetEsn() {
  cout << "GetEsn" << endl;

  return "12345";
}

DBus::Struct<uint32_t, string, uint32_t> DummyModem::GetServingSystem() {
  DBus::Struct<uint32_t, string, uint32_t> result;

  cout << "GetServingSystem" << endl;

  return result;
}

void DummyModem::GetRegistrationState(uint32_t& cdma_1x_state, uint32_t& evdo_state) {
  cout << "GetRegistrationState" << endl;
}
