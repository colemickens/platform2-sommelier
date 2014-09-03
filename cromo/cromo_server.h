// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_CROMO_SERVER_H_
#define CROMO_CROMO_SERVER_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>
#include <metrics/metrics_library.h>

#include "cromo/carrier.h"
#include "cromo/hooktable.h"
#include "dbus_adaptors/org.freedesktop.ModemManager.h"

class ModemHandler;

// Implements the ModemManager DBus API, and manages the
// modem manager instances that handle specific types of
// modems.
class CromoServer : public org::freedesktop::ModemManager_adaptor,
                    public DBus::IntrospectableAdaptor,
                    public DBus::ObjectAdaptor {
 public:
  explicit CromoServer(DBus::Connection& connection);  // NOLINT - refs.
  ~CromoServer();

  void AddModemHandler(ModemHandler* handler);
  void CheckForPowerDaemon();

  // .*Carrier.* are exported to plugins.  See Makefile for details
  void AddCarrier(Carrier* carrier);
  Carrier* FindCarrierByCarrierId(carrier_id_t carrier_id_t);
  Carrier* FindCarrierByName(const std::string& carrier_name);
  // Returns a carrier for a modem class to use before it's figured
  // out a real carrier
  Carrier* FindCarrierNoOp();

  // ModemManager DBUS API methods.
  std::vector<DBus::Path> EnumerateDevices(
      DBus::Error& error);  // NOLINT - refs.
  void ScanDevices(DBus::Error& error) {}  // NOLINT - refs.
  void SetLogging(const std::string& level,
                  DBus::Error& error);  // NOLINT - refs.

  static const char* kServiceName;
  static const char* kServicePath;

  HookTable& start_exit_hooks() { return start_exit_hooks_; }
  HookTable& exit_ok_hooks() { return exit_ok_hooks_; }

 private:
  typedef std::map<std::string, Carrier*> CarrierMap;
  typedef std::vector<ModemHandler*> ModemHandlers;

  // The modem handlers that we are managing.
  ModemHandlers modem_handlers_;

  CarrierMap carriers_;
  scoped_ptr<Carrier> carrier_no_op_;

  HookTable start_exit_hooks_;
  HookTable exit_ok_hooks_;

  scoped_ptr<MetricsLibraryInterface> metrics_lib_;

  DISALLOW_COPY_AND_ASSIGN(CromoServer);
};

#endif  // CROMO_CROMO_SERVER_H_
