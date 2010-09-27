// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_SERVER_H_
#define CROMO_SERVER_H_

#include "base/basictypes.h"

#include <dbus-c++/glib-integration.h>
#include <dbus-c++/dbus.h>

#include "manager_server_glue.h"
#include "hooktable.h"

class ModemHandler;

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

  // ModemManager DBUS API methods.
  std::vector<DBus::Path> EnumerateDevices(DBus::Error& error);

  static const char* kServiceName;
  static const char* kServicePath;

  HookTable& start_exit_hooks() { return start_exit_hooks_; }
  HookTable& exit_ok_hooks() { return exit_ok_hooks_; }
  HookTable& suspend_ok_hooks() { return suspend_ok_hooks_; }

  // Registers a suspend delay. The maximum delay specified is the longest time
  // it will take before the caller's suspend_ok_hook will return true.
  void RegisterStartSuspend(const std::string& name, bool (*func)(void *),
                            void *arg, unsigned int maxdelay);
  void UnregisterStartSuspend(const std::string& name);

 private:
  // The modem handlers that we are managing.
  std::vector<ModemHandler*> modem_handlers_;
  HookTable start_exit_hooks_;
  HookTable exit_ok_hooks_;
  HookTable start_suspend_hooks_;
  HookTable suspend_ok_hooks_;

  DBus::Connection conn_;

  void PowerDaemonUp();
  void PowerDaemonDown();
  void SuspendDelay(unsigned int seqnum);

  typedef std::map<const std::string, unsigned int> SuspendDelayMap;

  SuspendDelayMap suspend_delays_;

  void SuspendReady();
  bool CheckSuspendReady();
  void RegisterSuspendDelay();

  unsigned int MaxSuspendDelay();

  bool powerd_up_;

  unsigned int max_suspend_delay_;
  unsigned int suspend_nonce_;

  DISALLOW_COPY_AND_ASSIGN(CromoServer);

  friend class MessageHandler;
  friend gboolean test_for_suspend(void *arg);
};

#endif // CROMO_SERVER_H_
