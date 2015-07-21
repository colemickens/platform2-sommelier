// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "easy-unlock/daemon.h"

#include <signal.h>
#include <sys/signalfd.h>

#include <base/bind.h>
#include <base/location.h>
#include <chromeos/asynchronous_signal_handler.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/message_loops/message_loop.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>

#include "easy-unlock/dbus_adaptor.h"
#include "easy-unlock/easy_unlock_service.h"

namespace easy_unlock {

namespace {

bool HandleSignal(const base::Closure& callback,
                  const struct signalfd_siginfo& info) {
  LOG(ERROR) << "Received signal: " << info.ssi_signo;
  callback.Run();
  return true;
}

}  // namespace

Daemon::Daemon(scoped_ptr<easy_unlock::Service> service_impl,
               const scoped_refptr<dbus::Bus>& bus,
               bool install_signal_handler)
    : service_impl_(service_impl.Pass()),
      termination_signal_handler_(new chromeos::AsynchronousSignalHandler()),
      bus_(bus),
     install_signal_handler_(install_signal_handler) {
  if (install_signal_handler_)
    SetupSignalHandlers();
}

Daemon::~Daemon() {
  if (install_signal_handler_)
    RevertSignalHandlers();
}

bool Daemon::Initialize() {
  InitializeDBus();

  adaptor_.reset(new DBusAdaptor(service_impl_.get()));
  adaptor_->ExportDBusMethods(easy_unlock_dbus_object_);
  TakeDBusServiceOwnership();
  return true;
}

void Daemon::Finalize() {
  bus_->ShutdownAndBlock();
}

void Daemon::InitializeDBus() {
  CHECK(bus_->Connect());
  CHECK(bus_->SetUpAsyncOperations());

  easy_unlock_dbus_object_ = bus_->GetExportedObject(
      dbus::ObjectPath(easy_unlock::kEasyUnlockServicePath));
}

void Daemon::TakeDBusServiceOwnership() {
  CHECK(bus_->RequestOwnershipAndBlock(kEasyUnlockServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << kEasyUnlockServiceName;
}

void Daemon::Quit() {
  chromeos::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&chromeos::MessageLoop::BreakLoop,
                 base::Unretained(chromeos::MessageLoop::current())));
}

void Daemon::SetupSignalHandlers() {
  termination_signal_handler_->Init();

  chromeos::AsynchronousSignalHandler::SignalHandler handler =
      base::Bind(&HandleSignal,
                 base::Bind(&Daemon::Quit, base::Unretained(this)));

  termination_signal_handler_->RegisterHandler(SIGTERM, handler);
  termination_signal_handler_->RegisterHandler(SIGINT, handler);
  termination_signal_handler_->RegisterHandler(SIGHUP, handler);
}

void Daemon::RevertSignalHandlers() {
  termination_signal_handler_->UnregisterHandler(SIGTERM);
  termination_signal_handler_->UnregisterHandler(SIGINT);
  termination_signal_handler_->UnregisterHandler(SIGHUP);
}

}  // namespace easy_unlock
