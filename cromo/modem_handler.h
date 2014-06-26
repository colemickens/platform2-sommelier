// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_MODEM_HANDLER_H_
#define CROMO_MODEM_HANDLER_H_

#include "base/basictypes.h"

#include <dbus-c++/dbus.h>

typedef std::map<DBus::Path, DBus::ObjectAdaptor*> ModemMap;

class CromoServer;

class ModemHandler {
 public:
  // Constructor. |tag| is a vendor-specific tag supplied
  // by each sub-class to identify each class of modem
  // uniquely. The tag becomes part of the dbus path
  // used externally to name each modem object. 
  ModemHandler(CromoServer& server, const std::string& tag);
  virtual ~ModemHandler() {}
  virtual bool Initialize() = 0;

  virtual std::vector<DBus::Path> EnumerateDevices(DBus::Error& error) = 0;

  std::string vendor_tag() const { return vendor_tag_; }
  CromoServer& server() const { return server_; }

 protected:
  // RegisterSelf() must be called by all subclasses to complete
  // initialization and make the modem handler known to cromo.
  void RegisterSelf();
  std::string MakePath();

 private:
  CromoServer& server_;
  // Per-vendor tag used to construct unique DBus paths
  // for modem objects.
  std::string vendor_tag_;
  unsigned int instance_number_;

  DISALLOW_COPY_AND_ASSIGN(ModemHandler);
};

#endif // CROMO_MODEM_HANDLER_H_
