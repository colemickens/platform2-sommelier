// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_GOBI_MODEM_HANDLER_H_
#define CROMO_GOBI_MODEM_HANDLER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

#include <map>

#include <dbus-c++/types.h>

#include <cromo/modem_handler.h>
#include "gobi_modem.h"

class DeviceWatcher;
namespace gobi {
class Sdk;
}

class GobiModemHandler : public ModemHandler {
 public:
  explicit GobiModemHandler(CromoServer& server);
  virtual ~GobiModemHandler();

  virtual std::vector<DBus::Path> EnumerateDevices(DBus::Error& error);
  virtual bool Initialize();
  void HandleUdevMessage(const char *action, const char *device);
  void HandlePollEvent();

  GobiModem* LookupByPath(const std::string& path);
  void Remove(GobiModem *modem);

  // Exit, but do not clean up list of modems; the upstart script will
  // then clean up any modems we were servicing
  void ExitLeavingModemsForCleanup();

 private:
  typedef std::map<std::string, GobiModem *> KeyToModem;

  bool DevicePresent(const char *device);

  GobiModem* RemoveDeviceByName(const char *device);
  KeyToModem::iterator RemoveDeviceByIterator(KeyToModem::iterator p);

  // On clean exit clear the list of devices that need to be reset
  void ClearDeviceListFile();

  // Write a list of devices to a file so that upstart can reset the
  // devices if we exit unexpectedly.
  void WriteDeviceListFile(const KeyToModem &modems);

  bool GetDeviceList();
  void MonitorDevices();

  bool clear_device_list_on_destroy_;
  DeviceWatcher *device_watcher_;
  KeyToModem key_to_modem_;
  int scan_generation_;

  scoped_ptr<gobi::Sdk> sdk_;

  DISALLOW_COPY_AND_ASSIGN(GobiModemHandler);
};


#endif  // CROMO_GOBI_MODEM_HANDLER_H_
