// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_LAUNCHER_CONSTANTS_H_
#define VM_TOOLS_LAUNCHER_CONSTANTS_H_

namespace vm_tools {
namespace launcher {

// Path to lkvm binary.
constexpr char kLkvmBin[] = "/usr/bin/lkvm";

// Path to the VM kernel image.
constexpr char kVmKernelPath[] = "/opt/google/vm/vm_kernel";

// Path to the VM rootfs image.
constexpr char kVmRootfsPath[] = "/opt/google/vm/vm_rootfs.img";

// Runtime directory where information about running VMs will be stored.
constexpr char kVmRuntimeDirectory[] = "/run/vm";

}  // namespace launcher
}  // namespace vm_tools

#endif  // VM_TOOLS_LAUNCHER_CONSTANTS_H_
