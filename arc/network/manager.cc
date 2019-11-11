// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/manager.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/unix_domain_socket.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/minijail/minijail.h>

#include "arc/network/guest_events.h"
#include "arc/network/ipc.pb.h"

namespace arc_networkd {
namespace {

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns nullptr, an empty response is
// created and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(std::move(response));
}

}  // namespace

Manager::Manager(std::unique_ptr<HelperProcess> adb_proxy,
                 std::unique_ptr<HelperProcess> mcast_proxy,
                 std::unique_ptr<HelperProcess> nd_proxy)
    : adb_proxy_(std::move(adb_proxy)),
      mcast_proxy_(std::move(mcast_proxy)),
      nd_proxy_(std::move(nd_proxy)),
      addr_mgr_({
          AddressManager::Guest::ARC,
          AddressManager::Guest::ARC_NET,
      }),
      gsock_(AF_UNIX, SOCK_DGRAM) {
  runner_ = std::make_unique<MinijailedProcessRunner>();
  datapath_ = std::make_unique<Datapath>(runner_.get());
}

int Manager::OnInit() {
  prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

  // Handle subprocess lifecycle.
  process_reaper_.Register(this);

  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, adb_proxy_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 adb_proxy_->pid())))
      << "Failed to watch adb-proxy child process";
  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, mcast_proxy_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 nd_proxy_->pid())))
      << "Failed to watch multicast-proxy child process";
  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, nd_proxy_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 nd_proxy_->pid())))
      << "Failed to watch nd-proxy child process";

  // Setup the socket for guests to connect and notify certain events.
  // TODO(garrick): Remove once DBus API available.
  struct sockaddr_un addr = {0};
  socklen_t addrlen = 0;
  FillGuestSocketAddr(&addr, &addrlen);
  if (!gsock_.Bind((const struct sockaddr*)&addr, addrlen)) {
    LOG(ERROR) << "Cannot bind guest socket @" << kGuestSocketPath
               << "; exiting";
    return -1;
  }

  // TODO(garrick): Remove this workaround ASAP.
  // Handle signals for ARC lifecycle.
  RegisterHandler(SIGUSR1,
                  base::Bind(&Manager::OnSignal, base::Unretained(this)));
  RegisterHandler(SIGUSR2,
                  base::Bind(&Manager::OnSignal, base::Unretained(this)));
  gsock_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      gsock_.fd(), base::BindRepeating(&Manager::OnFileCanReadWithoutBlocking,
                                       base::Unretained(this)));

  // Run after Daemon::OnInit().
  base::MessageLoopForIO::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Manager::InitialSetup, weak_factory_.GetWeakPtr()));

  return DBusDaemon::OnInit();
}

void Manager::InitialSetup() {
  LOG(INFO) << "Setting up DBus service interface";
  dbus_svc_path_ = bus_->GetExportedObject(
      dbus::ObjectPath(patchpanel::kPatchPanelServicePath));
  if (!dbus_svc_path_) {
    LOG(FATAL) << "Failed to export " << patchpanel::kPatchPanelServicePath
               << " object";
  }

  using ServiceMethod =
      std::unique_ptr<dbus::Response> (Manager::*)(dbus::MethodCall*);
  const std::map<const char*, ServiceMethod> kServiceMethods = {
      {patchpanel::kArcStartupMethod, &Manager::OnArcStartup},
      {patchpanel::kArcShutdownMethod, &Manager::OnArcShutdown},
      {patchpanel::kArcVmStartupMethod, &Manager::OnArcVmStartup},
      {patchpanel::kArcVmShutdownMethod, &Manager::OnArcVmShutdown},
  };

  for (const auto& kv : kServiceMethods) {
    if (!dbus_svc_path_->ExportMethodAndBlock(
            patchpanel::kPatchPanelInterface, kv.first,
            base::Bind(&HandleSynchronousDBusMethodCall,
                       base::Bind(kv.second, base::Unretained(this))))) {
      LOG(FATAL) << "Failed to export method " << kv.first;
    }
  }

  if (!bus_->RequestOwnershipAndBlock(patchpanel::kPatchPanelServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(FATAL) << "Failed to take ownership of "
               << patchpanel::kPatchPanelServiceName;
  }
  LOG(INFO) << "DBus service interface ready";

  device_mgr_ = std::make_unique<DeviceManager>(
      std::make_unique<ShillClient>(bus_), &addr_mgr_, datapath_.get(),
      mcast_proxy_.get(), nd_proxy_.get());

  arc_svc_ = std::make_unique<ArcService>(device_mgr_.get(), datapath_.get());

  nd_proxy_->Listen();
}

void Manager::OnFileCanReadWithoutBlocking() {
  char buf[128] = {0};
  std::vector<base::ScopedFD> fds{};
  ssize_t len =
      base::UnixDomainSocket::RecvMsg(gsock_.fd(), buf, sizeof(buf), &fds);

  if (len <= 0) {
    PLOG(WARNING) << "Read failed";
    return;
  }

  // Only expecting ARCVM start/stop events from Concierge here.
  auto event = ArcGuestEvent::Parse(buf);
  if (!event || !event->isVm()) {
    LOG(WARNING) << "Unexpected message received: " << buf;
    return;
  }

  GuestMessage msg;
  msg.set_type(GuestMessage::ARC_VM);
  msg.set_arcvm_vsock_cid(event->id());
  msg.set_event(event->isStarting() ? GuestMessage::START : GuestMessage::STOP);
  SendGuestMessage(msg);
}

// TODO(garrick): Remove this workaround ASAP.
bool Manager::OnSignal(const struct signalfd_siginfo& info) {
  // Only ARC++ scripts send signals so nothing to do for VM.
  if (info.ssi_signo == SIGUSR1) {
    // For now this value is ignored and the service discovers the
    // container pid on its own. Later, this is arrive via DBus message.
    StartArc(0 /*pid*/);
  } else {
    StopArc();
  }

  // Stay registered.
  return false;
}

void Manager::OnShutdown(int* exit_code) {
  device_mgr_.reset();
}

void Manager::OnSubprocessExited(pid_t pid, const siginfo_t& info) {
  LOG(ERROR) << "Subprocess " << pid << " exited unexpectedly";
  Quit();
}

bool Manager::StartArc(pid_t pid) {
  // TOSO(garrick): Use pid.
  arc_svc_->OnStart();

  GuestMessage msg;
  msg.set_event(GuestMessage::START);
  msg.set_type(GuestMessage::ARC);
  msg.set_arc_pid(pid);
  SendGuestMessage(msg);

  return true;
}

void Manager::StopArc() {
  GuestMessage msg;
  msg.set_event(GuestMessage::STOP);
  msg.set_type(GuestMessage::ARC);
  SendGuestMessage(msg);

  arc_svc_->OnStop();
}

bool Manager::StartArcVm(int cid) {
  // TODO(garrick0: Start ARCVM

  GuestMessage msg;
  msg.set_event(GuestMessage::START);
  msg.set_type(GuestMessage::ARC_VM);
  msg.set_arcvm_vsock_cid(cid);
  SendGuestMessage(msg);

  return true;
}

void Manager::StopArcVm() {
  GuestMessage msg;
  msg.set_event(GuestMessage::STOP);
  msg.set_type(GuestMessage::ARC_VM);
  SendGuestMessage(msg);

  // TODO(garrick): Stop ARCVM
}

std::unique_ptr<dbus::Response> Manager::OnArcStartup(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "ARC++ starting up";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  patchpanel::ArcStartupRequest request;
  patchpanel::ArcStartupResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse request";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  StartArc(request.pid());

  // TODO(garrick): create and start ArcService.
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Manager::OnArcShutdown(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "ARC++ shutting down";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  patchpanel::ArcShutdownRequest request;
  patchpanel::ArcShutdownResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse request";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  StopArc();

  // TODO(garrick): delete ArcService.
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Manager::OnArcVmStartup(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "ARCVM starting up";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  patchpanel::ArcVmStartupRequest request;
  patchpanel::ArcVmStartupResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse request";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  StartArcVm(request.cid());

  // TODO(garrick): create and start ArcVmService.
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Manager::OnArcVmShutdown(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "ARCVM shutting down";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  patchpanel::ArcVmShutdownRequest request;
  patchpanel::ArcVmShutdownResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse request";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  StopArcVm();

  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

void Manager::SendGuestMessage(const GuestMessage& msg) {
  IpHelperMessage ipm;
  *ipm.mutable_guest_message() = msg;
  adb_proxy_->SendMessage(ipm);
  mcast_proxy_->SendMessage(ipm);
  nd_proxy_->SendMessage(ipm);
}

}  // namespace arc_networkd
