// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "virtual_file_provider/service.h"

#include <fcntl.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>

#include <base/bind.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <dbus/bus.h>
#include <dbus/message.h>

namespace virtual_file_provider {

namespace {

// TODO(hashimoto): Share these constants with chrome.
// D-Bus service constants.
constexpr char kVirtualFileProviderInterface[] =
    "org.chromium.VirtualFileProviderInterface";
constexpr char kVirtualFileProviderServicePath[] =
    "/org/chromium/VirtualFileProvider";
constexpr char kVirtualFileProviderServiceName[] =
    "org.chromium.VirtualFileProvider";

// Method names.
constexpr char kOpenFileMethod[] = "OpenFile";

// Signal names.
constexpr char kReadRequestSignal[] = "ReadRequest";

}  // namespace

Service::Service(const base::FilePath& fuse_mount_path)
    : fuse_mount_path_(fuse_mount_path), weak_ptr_factory_(this) {
  thread_checker_.DetachFromThread();
}

Service::~Service() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bus_)
    bus_->ShutdownAndBlock();
}

bool Service::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Connect the bus.
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  if (!bus_->Connect()) {
    LOG(ERROR) << "Failed to initialize D-Bus connection.";
    return false;
  }
  // Export methods.
  exported_object_ = bus_->GetExportedObject(
      dbus::ObjectPath(kVirtualFileProviderServicePath));
  if (!exported_object_->ExportMethodAndBlock(
          kVirtualFileProviderInterface,
          kOpenFileMethod,
          base::Bind(&Service::OpenFile, weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Failed to export OpenFile method.";
    return false;
  }
  // Request the ownership of the service name.
  if (!bus_->RequestOwnershipAndBlock(kVirtualFileProviderServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to own the service name";
    return false;
  }
  return true;
}

void Service::SendReadRequest(const std::string& id,
                              int64_t offset,
                              int64_t size,
                              base::ScopedFD fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  dbus::Signal signal(kVirtualFileProviderInterface, kReadRequestSignal);

  dbus::MessageWriter writer(&signal);
  writer.AppendString(id);
  writer.AppendInt64(offset);
  writer.AppendInt64(size);
  dbus::FileDescriptor dbus_fd(fd.get());
  dbus_fd.CheckValidity();
  writer.AppendFileDescriptor(dbus_fd);
  exported_object_->SendSignal(&signal);
}

void Service::OpenFile(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Generate a new ID.
  std::string id = base::GenerateGUID();
  // An ID corresponds to a file name in the FUSE file system.
  base::FilePath path = fuse_mount_path_.AppendASCII(id);
  // Create a new FD associated with the ID.
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(),
                                      O_RDONLY | O_CLOEXEC)));

  // Send response.
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString(id);
  dbus::FileDescriptor dbus_fd(fd.get());
  dbus_fd.CheckValidity();
  writer.AppendFileDescriptor(dbus_fd);
  response_sender.Run(std::move(response));
}

}  // namespace virtual_file_provider
