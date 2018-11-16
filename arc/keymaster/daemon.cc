// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/keymaster/daemon.h"

#include <sysexits.h>

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <chromeos/dbus/service_constants.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/strong_binding.h>

#include "arc/keymaster/keymaster_server.h"

namespace arc {
namespace keymaster {

namespace {

void InitMojo() {
  mojo::edk::Init();
  mojo::edk::InitIPCSupport(base::ThreadTaskRunnerHandle::Get());
  LOG(INFO) << "Mojo init succeeded.";
}

}  // anonymous namespace

Daemon::Daemon() : weak_factory_(this) {}
Daemon::~Daemon() = default;

int Daemon::OnInit() {
  int exit_code = brillo::DBusDaemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;

  InitMojo();
  InitDBus();
  return EX_OK;
}

void Daemon::InitDBus() {
  dbus::ExportedObject* exported_object =
      bus_->GetExportedObject(dbus::ObjectPath(kArcKeymasterServicePath));

  CHECK(exported_object);
  CHECK(exported_object->ExportMethodAndBlock(
      kArcKeymasterInterfaceName, kBootstrapMojoConnectionMethod,
      base::Bind(&Daemon::BootstrapMojoConnection,
                 weak_factory_.GetWeakPtr())));
  CHECK(bus_->RequestOwnershipAndBlock(kArcKeymasterServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY));
  LOG(INFO) << "D-Bus registration succeeded";
}

void Daemon::BootstrapMojoConnection(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  LOG(INFO) << "Receiving bootstrap mojo call from D-Bus client.";

  if (is_bound_) {
    LOG(WARNING) << "Trying to instantiate multiple Mojo proxies.";
    return;
  }

  base::ScopedFD file_handle;
  dbus::MessageReader reader(method_call);

  if (!reader.PopFileDescriptor(&file_handle)) {
    LOG(ERROR) << "Couldn't extract Mojo IPC handle.";
    return;
  }

  if (!file_handle.is_valid()) {
    LOG(ERROR) << "Couldn't get file handle sent over D-Bus.";
    return;
  }

  if (!base::SetCloseOnExec(file_handle.get())) {
    PLOG(ERROR) << "Failed setting FD_CLOEXEC on fd.";
    return;
  }

  AcceptProxyConnection(std::move(file_handle));
  LOG(INFO) << "Mojo connection established.";
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void Daemon::AcceptProxyConnection(base::ScopedFD fd) {
  mojo::edk::SetParentPipeHandle(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(fd.release())));
  mojo::ScopedMessagePipeHandle child_pipe =
      mojo::edk::CreateChildMessagePipe("arc-keymaster-pipe");
  mojo::MakeStrongBinding(
      std::make_unique<KeymasterServer>(),
      mojo::MakeRequest<arc::mojom::KeymasterServer>(std::move(child_pipe)));
  is_bound_ = true;
}

}  // namespace keymaster
}  // namespace arc
