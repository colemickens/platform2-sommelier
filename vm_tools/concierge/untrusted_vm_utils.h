// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_UNTRUSTED_VM_UTILS_H_
#define VM_TOOLS_CONCIERGE_UNTRUSTED_VM_UTILS_H_

#include <utility>

#include <base/files/file_path.h>
#include <base/optional.h>
#include <dbus/object_proxy.h>

namespace vm_tools {
namespace concierge {

// Used to represent kernel version.
using KernelVersionAndMajorRevision = std::pair<int, int>;

// Used to check for, and if needed enable, the conditions required for
// untrusted VMs.
class UntrustedVMUtils {
 public:
  // |debugd_proxy| - Used to call into debugd daemon.
  // |host_kernel_version| - Kernel version of the host.
  // |min_needed_version| - Minimum kernel version required to support untrusted
  // VMs.
  // |l1tf_status_path| - Path to read L1TF vulnerability status from.
  // |mds_status_path| - Path to read MDS vulnerability status from.
  UntrustedVMUtils(dbus::ObjectProxy* debugd_proxy,
                   KernelVersionAndMajorRevision host_kernel_version,
                   KernelVersionAndMajorRevision min_needed_version,
                   const base::FilePath& l1tf_status_path,
                   const base::FilePath& mds_status_path);

  // Mitigation status for L1TF and MDS vulnerabilities.
  enum class MitigationStatus {
    // The host is not vulnerable.
    NOT_VULNERABLE,

    // The host is vulnerable.
    VULNERABLE,

    // The host is vulnerable but can be secure if SMT is disabled on the host.
    VULNERABLE_DUE_TO_SMT_ENABLED,
  };

  // Returns the mitigation status for untrusted VMs based on the following
  // checks
  // - Check if kernel version >= |min_needed_version_|.
  // - Check if L1TF is mitigated.
  // - Check if MDS is mitigated.
  MitigationStatus CheckUntrustedVMMitigationStatus();

  // Disables SMT on the host it's run on. Returns true if successful or if SMT
  // was already disabled. Returns false otherwise.
  bool DisableSMT();

  // Sets |host_kernel_version_| for testing.
  void SetKernelVersionForTesting(
      KernelVersionAndMajorRevision host_kernel_version);

 private:
  // Not owned. Used for calling the debugd API.
  dbus::ObjectProxy* const debugd_proxy_;

  // Kernel version of the host this class runs on.
  KernelVersionAndMajorRevision host_kernel_version_;

  // Minimum kernel version required to support untrusted VMs.
  KernelVersionAndMajorRevision min_needed_version_;

  // Path to read L1TF vulnerability status from.
  base::FilePath l1tf_status_path_;

  // Path to read MDS vulnerability status from.
  base::FilePath mds_status_path_;

  DISALLOW_COPY_AND_ASSIGN(UntrustedVMUtils);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_UNTRUSTED_VM_UTILS_H_
