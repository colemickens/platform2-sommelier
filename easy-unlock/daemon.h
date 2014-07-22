// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASY_UNLOCK_DAEMON_H_
#define EASY_UNLOCK_DAEMON_H_

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace chromeos {
class AsynchronousSignalHandler;
}  // namespace chromeos

namespace dbus {
class Bus;
class ExportedObject;
}  // namespace dbus

namespace easy_unlock {
class DBusAdaptor;
class Service;
}  // namespace easy_unlock

namespace easy_unlock {

// The EasyUnlock dbus service daemon. Initialized and shutdown from main.
class Daemon {
 public:
  Daemon(scoped_ptr<easy_unlock::Service> service_impl,
         const scoped_refptr<dbus::Bus>& bus,
         const base::Closure& quit_closure,
         bool install_signal_handler);
  ~Daemon();

  // Initializes the dbus service daemon
  bool Initialize();

  // Shuts down the dbus service.
  void Finalize();

 private:
  void InitializeDBus();
  void TakeDBusServiceOwnership();

  // Post quit closure task to the message loop the daemon is started on.
  void Quit();

  // Sets up termination signal handlers.
  void SetupSignalHandlers();

  // Called when the Daemon is destructed. Resets signal handers set in
  // |SetupSignalHandlers|.
  void RevertSignalHandlers();

  scoped_ptr<easy_unlock::Service> service_impl_;
  scoped_ptr<DBusAdaptor> adaptor_;

  base::Closure quit_closure_;
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;

  // Handler for termination signals. The handled signals cause |Quit| to get
  // called.
  scoped_ptr<chromeos::AsynchronousSignalHandler> termination_signal_handler_;

  scoped_refptr<dbus::Bus> bus_;
  // Owned by |bus_|.
  dbus::ExportedObject* easy_unlock_dbus_object_;

  bool install_signal_handler_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace easy_unlock

#endif  // EASY_UNLOCK_DAEMON_H_
