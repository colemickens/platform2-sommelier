// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_SERVER_H_
#define CROMO_SERVER_H_

#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

#include <dbus-c++/glib-integration.h>
#include <dbus-c++/dbus.h>
#include <metrics/metrics_library.h>

#include "manager_server_glue.h"
#include "hooktable.h"

class ModemHandler;
class Carrier;

// Implements the ModemManager DBus API, and manages the
// modem manager instances that handle specific types of
// modems.
class CromoServer
    : public org::freedesktop::ModemManager_adaptor,
      public DBus::IntrospectableAdaptor,
      public DBus::ObjectAdaptor {
 public:
  explicit CromoServer(DBus::Connection& connection);
  ~CromoServer();

  void AddModemHandler(ModemHandler* handler);
  void CheckForPowerDaemon();

  // .*Carrier.* are exported to plugins.  See Makefile for details
  void AddCarrier(Carrier *carrier);
  Carrier *FindCarrierByCarrierId(unsigned long carrier_id);
  Carrier *FindCarrierByName(const std::string &carrier_name);
  // Returns a carrier for a modem class to use before it's figured
  // out a real carrier
  Carrier *FindCarrierNoOp();

  // ModemManager DBUS API methods.
  std::vector<DBus::Path> EnumerateDevices(DBus::Error& error);
  void ScanDevices(DBus::Error& error) {}
  void SetLogging(const std::string& level, DBus::Error& error);

  static const char* kServiceName;
  static const char* kServicePath;

  HookTable& start_exit_hooks() { return start_exit_hooks_; }
  HookTable& exit_ok_hooks() { return exit_ok_hooks_; }
  HookTable& suspend_ok_hooks() { return suspend_ok_hooks_; }
  HookTable& on_suspended_hooks() { return on_suspended_hooks_; }
  HookTable& on_resumed_hooks() { return on_resumed_hooks_; }

  // Registers a suspend delay. The maximum delay specified is the longest time
  // it will take before the caller's suspend_ok_hook will return true.
  void RegisterStartSuspend(const std::string& name, bool (*func)(void *),
                            void *arg, unsigned int maxdelay);
  void UnregisterStartSuspend(const std::string& name);

 private:
  typedef std::map<std::string, Carrier*> CarrierMap;
  typedef std::vector<ModemHandler*> ModemHandlers;

  // The modem handlers that we are managing.
  ModemHandlers modem_handlers_;

  CarrierMap carriers_;
  scoped_ptr<Carrier> carrier_no_op_;

  HookTable start_exit_hooks_;
  HookTable exit_ok_hooks_;

  HookTable start_suspend_hooks_;
  HookTable suspend_ok_hooks_;

  HookTable on_suspended_hooks_;
  HookTable on_resumed_hooks_;

  DBus::Connection conn_;

  void PowerDaemonUp();
  void PowerDaemonDown();
  void PowerStateChanged(const std::string& new_power_state);
  void SuspendDelay(unsigned int seqnum);

  typedef std::map<const std::string, unsigned int> SuspendDelayMap;

  SuspendDelayMap suspend_delays_;

  void SuspendReady();
  bool CheckSuspendReady();
  static gboolean RegisterSuspendDelayCallback(void *arg);
  void RegisterSuspendDelay();
  void CancelSuspendCompletionTimeout();

  unsigned int MaxSuspendDelay();

  bool powerd_up_;

  unsigned int max_suspend_delay_;
  unsigned int suspend_nonce_;
  guint suspend_completion_timeout_;

  scoped_ptr<MetricsLibraryInterface> metrics_lib_;
  unsigned long long suspend_start_time_;

  DISALLOW_COPY_AND_ASSIGN(CromoServer);

  friend class MessageHandler;
  friend gboolean AssumeSuspendCancelled(void *arg);
  friend gboolean test_for_suspend(void *arg);
};

#endif // CROMO_SERVER_H_
