// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_GOBI_MODEM_HANDLER_H_
#define CROMO_GOBI_MODEM_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

#include <map>

#include <dbus-c++/types.h>

#include <cromo/modem_handler.h>
#include "gobi_modem.h"

class DeviceWatcher;
namespace gobi {
class Sdk;
}

// This class cares about two orthogonal kinds of path: control paths and DBus
// paths. Control paths are only used internally, and are the names of the qcqmi
// device the modem is associated with (e.g., /dev/qcqmi0); DBus paths are used
// externally and are associated with DBus objects (e.g.
// /org/chromium/ModemManager/Gobi/0). Public methods always deal in DBus paths,
// and private methods always deal in control paths.
class GobiModemHandler : public ModemHandler {
 public:
  explicit GobiModemHandler(CromoServer& server);
  virtual ~GobiModemHandler();

  virtual std::vector<DBus::Path> EnumerateDevices(DBus::Error& error);
  virtual bool Initialize();
  void HandleUdevMessage(const char *action, const char *device);
  void HandlePollEvent();

  void ExitLeavingModemsForCleanup();
  GobiModem* LookupByDbusPath(const std::string& dbuspath);
  void Remove(GobiModem *m);

 private:
  typedef std::map<std::string, GobiModem *> ControlPathToModem;

  bool DevicePresentByControlPath(const char *path);
  void RemoveDeviceByControlPath(const char *path);

  ControlPathToModem::iterator
      RemoveDeviceByIterator(ControlPathToModem::iterator p);

  // On clean exit clear the list of devices that need to be reset
  void ClearDeviceListFile();

  // Write a list of devices to a file so that upstart can reset the
  // devices if we exit unexpectedly.
  void WriteDeviceListFile(const ControlPathToModem &modems);

  bool GetDeviceList();
  void MonitorDevices();
  ControlPathToModem control_path_to_modem_;

  bool clear_device_list_on_destroy_;
  DeviceWatcher *device_watcher_;
  ControlPathToModem key_to_modem_;
  int scan_generation_;

  scoped_ptr<gobi::Sdk> sdk_;

  DISALLOW_COPY_AND_ASSIGN(GobiModemHandler);
};


#endif  // CROMO_GOBI_MODEM_HANDLER_H_
