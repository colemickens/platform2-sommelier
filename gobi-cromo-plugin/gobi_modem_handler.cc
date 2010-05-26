// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem_handler.h"

#include <iostream>

#include <glog/logging.h>
#include <base/scoped_ptr.h>

#include <cromo/cromo_server.h>
#include <cromo/plugin.h>

#include "gobi_modem.h"

using std::vector;
using std::cout;
using std::cerr;
using std::endl;

static gobi::Sdk GOBI_SDK;


GobiModemHandler::GobiModemHandler(CromoServer& server)
    : ModemHandler(server, "Gobi"),
      scan_generation_(0) {
}

bool GobiModemHandler::Initialize() {
  std::vector<DEVICE_ELEMENT> available =
      GetAvailableDevices();
  HandleNewDeviceList(available);
  RegisterSelf();
  return true;
}

// Enumerate the existing devices, and add them to the list of devices
// that are managed by the ChromeOS modem manager

vector<DBus::Path> GobiModemHandler::EnumerateDevices() {
  vector <DBus::Path> to_return;
  std::vector<DEVICE_ELEMENT> available =
      GetAvailableDevices();
  HandleNewDeviceList(available);
  for (KeyToModem::iterator p = key_to_modem_.begin();
       p != key_to_modem_.end(); ++p) {
    to_return.push_back(p->second->path());
  }
  return to_return;
}

void GobiModemHandler::HandleNewDeviceList(
    const std::vector<DEVICE_ELEMENT> &incoming) {

  ++scan_generation_;

  for (size_t i = 0; i < incoming.size(); ++i) {
    KeyToModem::iterator p = key_to_modem_.find(incoming[i].deviceKey);
    if (p != key_to_modem_.end()) {
      CHECK((*p).second);
      (*p).second->set_last_seen(scan_generation_);
    } else {
      LOG(INFO) << "Found new modem: " << incoming[i].deviceNode;
      GobiModem *m = new GobiModem(server().conn(),
                                   MakePath(),
                                   this,
                                   incoming[i],
                                   &GOBI_SDK);
      m->set_last_seen(scan_generation_);
      key_to_modem_[std::string(incoming[i].deviceKey)] = m;
      AddModem(m);
    }
  }

  for (KeyToModem::iterator p = key_to_modem_.begin();
       p != key_to_modem_.end(); ++p) {
    if (((*p).second)->last_seen() != scan_generation_) {
      // TODO(rochberg):  cleanly delete
      // TODO(rochberg):  do not delete modems that are resetting
      // TODO(rochberg):  send RemoveDevice
      CHECK(0);
    }
  }
}


std::vector<DEVICE_ELEMENT> GobiModemHandler::GetAvailableDevices() {
  const int MAX_MODEMS = 16;

  ULONG rc;
  vector<DBus::Path> paths;

  scoped_array<DEVICE_ELEMENT> devices(new DEVICE_ELEMENT[MAX_MODEMS]);
  int numDevices = MAX_MODEMS;

  rc = GOBI_SDK.QCWWANEnumerateDevices(reinterpret_cast<BYTE *>(&numDevices),
                                       reinterpret_cast<BYTE *>(devices.get()));
  if (rc != 0) {
    // TODO(rochberg): ERROR
    CHECK(rc == 0);
  }

  LOG(INFO) << "Found " << numDevices << " gobi modems";

  return std::vector<DEVICE_ELEMENT>(devices.get(), devices.get() + numDevices);
}

static void onload(CromoServer* server) {
  cout << __FILE__ << ": onload() called" << endl;
  ModemHandler* mm = new GobiModemHandler(*server);
  if (!mm->Initialize())
    cerr << "Failed to initialize GobiModemHandler" << endl;
}

static void onunload() {
  // TODO(rochberg):  Disconnect once for good measure?
}

CROMO_DEFINE_PLUGIN(gobi, onload, onunload)
