// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midis/client_tracker.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/location.h>
#include <base/memory/ptr_util.h>
#include <brillo/message_loops/message_loop.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/binding.h>

namespace {

constexpr char kMidisPipe[] = "arc-midis-pipe";
constexpr char kSocketPath[] = "/run/midis/midis_socket";
const int kUnixNamedSocketBacklog = 5;

// Implementation of the MidisHost interface. This is used to
// get the actual MidisManager interface which is used by the client to
// communicate with midis.
// A request to initialize this should be initiated by the ArcBridgeHost.
//
// NOTE: It is expected that this class should only be instantiated once
// during the lifetime of the service. An error in the Message Pipe associated
// with this class is most likely an unrecoverable error, and will necessitate
// the restart of the midis service from Chrome.
class MidisHostImpl : public arc::mojom::MidisHost {
 public:
  // |client_tracker|, which must outlive MidisHostImpl, is owned by the caller.
  MidisHostImpl(arc::mojom::MidisHostRequest request,
                midis::ClientTracker* client_tracker);
  ~MidisHostImpl() override = default;

  // mojom::MidisHost:
  void Connect(arc::mojom::MidisServerRequest request,
               arc::mojom::MidisClientPtr client_ptr) override;

 private:
  // It's safe to hold a raw pointer to ClientTracker since we can assume
  // that the lifecycle of ClientTracker is a superset of the lifecycle of
  // MidisHostImpl.
  midis::ClientTracker* client_tracker_;
  mojo::Binding<arc::mojom::MidisHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(MidisHostImpl);
};

MidisHostImpl::MidisHostImpl(arc::mojom::MidisHostRequest request,
                             midis::ClientTracker* client_tracker)
    : client_tracker_(client_tracker), binding_(this, std::move(request)) {}

void MidisHostImpl::Connect(arc::mojom::MidisServerRequest request,
                            arc::mojom::MidisClientPtr client_ptr) {
  VLOG(1) << "Connect() called.";
  client_tracker_->MakeMojoClient(std::move(request), std::move(client_ptr));
}

}  // namespace

namespace midis {

void ClientTracker::MakeMojoClient(arc::mojom::MidisServerRequest request,
                                   arc::mojom::MidisClientPtr client_ptr) {
  client_id_counter_++;
  VLOG(1) << "MakeMojoClient called.";
  std::unique_ptr<Client> new_cli = Client::CreateMojo(
      device_tracker_,
      client_id_counter_,
      base::Bind(&ClientTracker::RemoveClient, weak_factory_.GetWeakPtr()),
      std::move(request),
      std::move(client_ptr));

  if (new_cli) {
    clients_.emplace(client_id_counter_, std::move(new_cli));
  }
}

ClientTracker::ClientTracker() : client_id_counter_(0), weak_factory_(this) {}

ClientTracker::~ClientTracker() {
  for (auto& client : clients_) {
    device_tracker_->RemoveClientFromDevices(client.first);
  }
  clients_.clear();
}

bool ClientTracker::InitClientTracker(DeviceTracker* device_tracker) {
  device_tracker_ = device_tracker;
  server_fd_ =
      base::ScopedFD(socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
  if (!server_fd_.is_valid()) {
    LOG(ERROR) << "Error creating midis server socket.";
    return false;
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  std::string sock_path = basedir_.value() + kSocketPath;
  strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path));

  // Calling fchmod before bind prevents the file from being modified.
  // Once bind() completes, we can change the permissions to allow
  // other users in the group to access the socket.
  if (fchmod(server_fd_.get(), 0700) < 0) {
    PLOG(ERROR) << "fchmod() on socket failed.";
    return false;
  }

  if (bind(server_fd_.get(),
           reinterpret_cast<sockaddr*>(&addr),
           sizeof(addr)) == -1) {
    PLOG(ERROR) << "bind() failed.";
    return false;
  }

  // Allow other group members to access MIDI devices.
  if (chmod(addr.sun_path, 0770) != 0) {
    PLOG(ERROR) << "chmod() on socket failed.";
    return false;
  }

  if (listen(server_fd_.get(), kUnixNamedSocketBacklog) == -1) {
    PLOG(ERROR) << "listen() failed.";
    return false;
  }

  VLOG(1) << "Start client server.";

  mojo::edk::Init();
  mojo::edk::InitIPCSupport(this, base::ThreadTaskRunnerHandle::Get());

  auto taskid = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE,
      server_fd_.get(),
      brillo::MessageLoop::kWatchRead,
      true,
      base::Bind(&ClientTracker::ProcessClient,
                 weak_factory_.GetWeakPtr(),
                 server_fd_.get()));

  return taskid != brillo::MessageLoop::kTaskIdNull;
}

void ClientTracker::ProcessClient(int fd) {
  auto new_fd = base::ScopedFD(accept(fd, NULL, NULL));
  if (!new_fd.is_valid()) {
    PLOG(ERROR) << "accept() failed.";
    return;
  }

  client_id_counter_++;
  std::unique_ptr<Client> new_cli = Client::Create(
      std::move(new_fd),
      device_tracker_,
      client_id_counter_,
      base::Bind(&ClientTracker::RemoveClient, weak_factory_.GetWeakPtr()));
  if (new_cli) {
    clients_.emplace(client_id_counter_, std::move(new_cli));
  }
}

void ClientTracker::RemoveClient(uint32_t client_id) {
  // First delete all references to this device.
  device_tracker_->RemoveClientFromDevices(client_id);
  clients_.erase(client_id);
}

void ClientTracker::AcceptProxyConnection(base::ScopedFD fd) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  mojo::edk::SetParentPipeHandle(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(fd.release())));
  mojo::ScopedMessagePipeHandle child_pipe =
      mojo::edk::CreateChildMessagePipe(kMidisPipe);
  midis_host_ = base::MakeUnique<MidisHostImpl>(
      mojo::MakeRequest<arc::mojom::MidisHost>(std::move(child_pipe)), this);
}

bool ClientTracker::IsProxyConnected() {
  return midis_host_ != nullptr;
}

void ClientTracker::OnShutdownComplete() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  mojo::edk::ShutdownIPCSupport();
}

}  // namespace midis
