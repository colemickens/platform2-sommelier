// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_VM_UTIL_H_
#define VM_TOOLS_CONCIERGE_VM_UTIL_H_

#include <sys/types.h>

#include <string>
#include <vector>

#include <base/time/time.h>
#include <vm_tools/concierge/usb_control.h>

namespace vm_tools {
namespace concierge {

// Path to the crosvm binary.
const char kCrosvmBin[] = "/usr/bin/crosvm";

// Calculates the amount of memory to give the virtual machine. Currently
// configured to provide 75% of system memory. This is deliberately over
// provisioned with the expectation that we will use the balloon driver to
// reduce the actual memory footprint.
std::string GetVmMemoryMiB();

// Sets the pgid of the current process to its pid.  This is needed because
// crosvm assumes that only it and its children are in the same process group
// and indiscriminately sends a SIGKILL if it needs to shut them down.
bool SetPgid();

// Waits for the |pid| to exit.  Returns true if |pid| successfully exited and
// false if it did not exit in time.
bool WaitForChild(pid_t child, base::TimeDelta timeout);

// Returns true if a process with |pid| exists.
bool CheckProcessExists(pid_t pid);

// Runs a crosvm subcommand.
void RunCrosvmCommand(std::string command, std::string socket_path);

// Attaches an usb device at host |bus|:|addr|, with |vid|, |pid| and an opened
// |fd|.
bool AttachUsbDevice(std::string socket_path,
                     uint8_t bus,
                     uint8_t addr,
                     uint16_t vid,
                     uint16_t pid,
                     int fd,
                     UsbControlResponse* response);

// Detaches the usb device at guest |port|.
bool DetachUsbDevice(std::string socket_path,
                     uint8_t port,
                     UsbControlResponse* response);

// Lists all usb devices attached to guest.
bool ListUsbDevice(std::string socket_path, std::vector<UsbDevice>* devices);

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_VM_UTIL_H_
