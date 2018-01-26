// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include <base/at_exit.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_piece.h>
#include <base/strings/string_split.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <vm_concierge/proto_bindings/service.pb.h>

using std::string;

namespace {

constexpr int kDefaultTimeoutMs = 30 * 1000;

// Converts an IPv4 address in network byte order into a string.
void IPv4AddressToString(uint32_t addr, string* address) {
  CHECK(address);

  char buf[INET_ADDRSTRLEN];
  struct in_addr in = {
      .s_addr = addr,
  };
  if (inet_ntop(AF_INET, &in, buf, sizeof(buf)) == nullptr) {
    PLOG(WARNING) << "Failed to convert " << addr << " into a string";
    return;
  }

  *address = buf;
}

int StartVm(dbus::ObjectProxy* proxy,
            string name,
            string kernel,
            string rootfs,
            string extra_disks) {
  if (name.empty()) {
    LOG(ERROR) << "--name is required";
    return -1;
  }

  if (kernel.empty()) {
    LOG(ERROR) << "--kernel is required";
    return -1;
  }

  if (rootfs.empty()) {
    LOG(ERROR) << "--rootfs is required";
    return -1;
  }

  if (!base::PathExists(base::FilePath(kernel))) {
    LOG(ERROR) << kernel << " does not exist";
    return -1;
  }

  if (!base::PathExists(base::FilePath(rootfs))) {
    LOG(ERROR) << rootfs << " does not exist";
    return -1;
  }

  LOG(INFO) << "Starting VM " << name << " with kernel " << kernel
            << " and rootfs " << rootfs;

  dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                               vm_tools::concierge::kStartVmMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::concierge::StartVmRequest request;
  request.set_name(std::move(name));

  request.mutable_vm()->set_kernel(std::move(kernel));
  request.mutable_vm()->set_rootfs(std::move(rootfs));

  for (base::StringPiece disk :
       base::SplitStringPiece(extra_disks, ":", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<base::StringPiece> tokens = base::SplitStringPiece(
        disk, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    // disk path[,writable[,mount target,fstype[,flags[,data]]]]
    if (tokens.empty()) {
      LOG(ERROR) << "Disk description is empty";
      return -1;
    }
    vm_tools::concierge::DiskImage* disk_image = request.add_disks();
    disk_image->set_path(tokens[0].data(), tokens[0].size());
    disk_image->set_do_mount(false);

    if (tokens.size() > 1) {
      int writable = 0;
      if (!base::StringToInt(tokens[1], &writable)) {
        LOG(ERROR) << "Unable to parse writable token: " << tokens[1];
        return -1;
      }

      disk_image->set_writable(writable != 0);
    }

    if (tokens.size() > 2) {
      if (tokens.size() == 3) {
        LOG(ERROR) << "Missing fstype for " << disk;
        return -1;
      }
      disk_image->set_mount_point(tokens[2].data(), tokens[2].size());
      disk_image->set_fstype(tokens[3].data(), tokens[3].size());
      disk_image->set_do_mount(true);
    }

    if (tokens.size() > 4) {
      uint64_t flags;
      if (!base::HexStringToUInt64(tokens[4], &flags)) {
        LOG(ERROR) << "Unable to parse flags: " << tokens[4];
        return -1;
      }

      disk_image->set_flags(flags);
    }

    if (tokens.size() > 5) {
      disk_image->set_data(tokens[5].data(), tokens[5].size());
    }

    if (!base::PathExists(base::FilePath(disk_image->path()))) {
      LOG(ERROR) << "Extra disk path does not exist: " << disk_image->path();
      return -1;
    }

    char flag_buf[20];
    snprintf(flag_buf, sizeof(flag_buf), "0x%x", disk_image->flags());

    LOG(INFO) << "Disk " << disk_image->path();
    LOG(INFO) << "    mnt point: " << disk_image->mount_point();
    LOG(INFO) << "    type:      " << disk_image->fstype();
    LOG(INFO) << "    flags:     " << flag_buf;
    LOG(INFO) << "    data:      " << disk_image->data();
    LOG(INFO) << "    writable:  " << disk_image->writable();
    LOG(INFO) << "    do_mount:  " << disk_image->do_mount();
  }

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode StartVmRequest protobuf";
    return -1;
  }

  std::unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send dbus message to concierge service";
    return -1;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::concierge::StartVmResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse response protobuf";
    return -1;
  }

  if (!response.success()) {
    LOG(ERROR) << "Failed to start VM: " << response.failure_reason();
    return -1;
  }

  vm_tools::concierge::VmInfo vm_info = response.vm_info();
  string address;
  IPv4AddressToString(vm_info.ipv4_address(), &address);

  LOG(INFO) << "Started VM with";
  LOG(INFO) << "    ip address: " << address;
  LOG(INFO) << "    vsock cid:  " << vm_info.cid();
  LOG(INFO) << "    process id: " << vm_info.pid();

  return 0;
}

int StopVm(dbus::ObjectProxy* proxy, string name) {
  if (name.empty()) {
    LOG(ERROR) << "--name is required";
    return -1;
  }

  LOG(INFO) << "Stopping VM " << name;

  dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                               vm_tools::concierge::kStopVmMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::concierge::StopVmRequest request;
  request.set_name(std::move(name));

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode StopVmRequest protobuf";
    return -1;
  }

  std::unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send dbus message to concierge service";
    return -1;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::concierge::StopVmResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse response protobuf";
    return -1;
  }

  if (!response.success()) {
    LOG(ERROR) << "Failed to stop VM: " << response.failure_reason();
    return -1;
  }

  LOG(INFO) << "Done";
  return 0;
}

int StopAllVms(dbus::ObjectProxy* proxy) {
  LOG(INFO) << "Stopping all VMs";

  dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                               vm_tools::concierge::kStopAllVmsMethod);

  std::unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send dbus message to concierge service";
    return -1;
  }

  LOG(INFO) << "Done";
  return 0;
}

int GetVmInfo(dbus::ObjectProxy* proxy, string name) {
  LOG(INFO) << "Getting VM info";

  dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                               vm_tools::concierge::kGetVmInfoMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::concierge::GetVmInfoRequest request;
  request.set_name(std::move(name));

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode GetVmInfo protobuf";
    return -1;
  }

  std::unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send dbus message to concierge service";
    return -1;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::concierge::GetVmInfoResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse response protobuf";
    return -1;
  }

  if (!response.success()) {
    LOG(ERROR) << "Failed to get VM info";
    return -1;
  }

  vm_tools::concierge::VmInfo vm_info = response.vm_info();
  string address;
  IPv4AddressToString(vm_info.ipv4_address(), &address);

  LOG(INFO) << "VM:           " << name;
  LOG(INFO) << "IPv4 address: " << address;
  LOG(INFO) << "pid:          " << vm_info.pid();
  LOG(INFO) << "vsock cid:    " << vm_info.cid();
  LOG(INFO) << "Done";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  // Operations.
  DEFINE_bool(start, false, "Start a VM");
  DEFINE_bool(stop, false, "Stop a running VM");
  DEFINE_bool(stop_all, false, "Stop all running VMs");
  DEFINE_bool(get_vm_info, false, "Get info for the given VM");

  // Parameters.
  DEFINE_string(kernel, "", "Path to the VM kernel");
  DEFINE_string(rootfs, "", "Path to the VM rootfs");
  DEFINE_string(name, "", "Name to assign to the VM");
  DEFINE_string(extra_disks, "",
                "Additional disk images to be mounted inside the VM");

  brillo::FlagHelper::Init(argc, argv, "vm_concierge client tool");
  brillo::InitLog(brillo::kLogToStderrIfTty);

  base::MessageLoopForIO message_loop;

  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(std::move(opts)));

  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return -1;
  }

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      vm_tools::concierge::kVmConciergeServiceName,
      dbus::ObjectPath(vm_tools::concierge::kVmConciergeServicePath));
  if (!proxy) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << vm_tools::concierge::kVmConciergeServiceName;
    return -1;
  }

  // The standard says that bool to int conversion is implicit and that
  // false => 0 and true => 1.
  if (FLAGS_start + FLAGS_stop + FLAGS_stop_all + FLAGS_get_vm_info != 1) {
    LOG(ERROR) << "Exactly one of --start, --stop, --stop_all, --get_vm_info"
               << "must be provided";
    return -1;
  }

  if (FLAGS_start) {
    return StartVm(proxy, std::move(FLAGS_name), std::move(FLAGS_kernel),
                   std::move(FLAGS_rootfs), std::move(FLAGS_extra_disks));
  } else if (FLAGS_stop) {
    return StopVm(proxy, std::move(FLAGS_name));
  } else if (FLAGS_stop_all) {
    return StopAllVms(proxy);
  } else if (FLAGS_get_vm_info) {
    return GetVmInfo(proxy, std::move(FLAGS_name));
  }

  // Unreachable.
  return 0;
}
