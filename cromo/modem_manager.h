// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_MODEM_MANAGER_H_
#define CROMO_MODEM_MANAGER_H_

#include "base/basictypes.h"

#include <dbus-c++/dbus.h>

typedef std::map<DBus::Path, DBus::ObjectAdaptor*> ModemMap;

class ModemManagerServer;

class ModemManager {
 public:
  // Constructor. |tag| is a vendor-specific tag supplied
  // by each sub-class to identify each class of modem
  // uniquely. The tag becomes part of the dbus path
  // used externally to name each modem object. 
  ModemManager(ModemManagerServer& server, const std::string& tag);
  virtual ~ModemManager();

  // Initialize() must be called by all subclasses to complete
  // initialization and make the modem manager known.
  virtual bool Initialize();

  virtual std::vector<DBus::Path> EnumerateDevices() = 0;

  // Adds a modem to the list of modems managed by this modem manager.
  // The modem must already have a unique path on the dbus.
  void AddModem(DBus::ObjectAdaptor* modem);
  void RemoveModem(DBus::ObjectAdaptor* modem);
  void ClearModemList();

  std::string vendor_tag() const { return vendor_tag_; }
  ModemManagerServer& server() const { return server_; }

 protected:
  std::string MakePath();

 private:
  ModemManagerServer& server_;
  // Per-vendor tag used to construct unique DBus paths
  // for modem objects.
  std::string vendor_tag_;
  unsigned int instance_number_;
  ModemMap modems_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager);
};

#endif // CROMO_MODEM_MANAGER_H_
