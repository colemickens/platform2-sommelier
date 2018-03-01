// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CrosVM manages a VM's lifecycle and associated resources.

#ifndef VM_TOOLS_LAUNCHER_CROSVM_H_
#define VM_TOOLS_LAUNCHER_CROSVM_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <brillo/process.h>

#include "vm_tools/launcher/mac_address.h"
#include "vm_tools/launcher/nfs_export.h"
#include "vm_tools/launcher/subnet.h"
#include "vm_tools/launcher/vsock_cid.h"

#include "vm_guest.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace launcher {

// This manages a VM instance, which consists of the resources allocated
// by the VM, as well as the VM process itself.
// CrosVM instances should be constructed with the factory methods Create (for
// new VM instances) and Load (for existing, running VM instances).
//
// Instances constructed by Create will by default shut down the VM and free
// all of its resources on destruction. Instances constructed by Load will by
// default leave the VM running. This behavior can be changed with
// SetReleaseOnDestruction, e.g. if a VM process wants to be allowed to run
// after vm_launcher exits.
class CrosVM {
 public:
  ~CrosVM();

  // Create a new CrosVM instance. This will allocate resources as necessary.
  // |name| should be unique among running VMs. |vm_kernel| and |vm_rootfs|
  // must be valid paths to a VM kernel and VM rootfs. Instances created
  // by this factory method are set to release on their destruction
  // by default.
  static std::unique_ptr<CrosVM> Create(const std::string& name,
                                        const base::FilePath& vm_kernel,
                                        const base::FilePath& vm_rootfs,
                                        const base::FilePath& nfs_path);

  // Load an existing CrosVM instance with the given |name|. Instances
  // created by this factory method are NOT set to release on their
  // destruction.
  static std::unique_ptr<CrosVM> Load(const std::string& name);

  // Get the VM name for a given VM pid.
  static bool GetNameForPid(pid_t target_pid, std::string* vm_name);

  // Start a VM asynchronously. This detaches the VM from stdin/stdout/stderr,
  // and allows vm_launcher to return immediately. If |ssh| is true, sshd
  // will be started in the VM. If a nonempty |container_disk| is supplied,
  // it will be attached to the VM  If |rw_container| is true, then the disk
  // will also be marked as writable.
  bool Start(bool ssh, const base::FilePath& container_disk, bool rw_container);

  // Run a VM. stdio will be given to the VM for serial console access, and
  // this method will block until the VM process exits. If |ssh| is true, sshd
  // will be started in the VM. If a nonempty |container_disk| is supplied,
  // it will be attached to the VM  If |rw_container| is true, then the disk
  // will also be marked as writable.
  bool Run(bool ssh, const base::FilePath& container_disk, bool rw_container);

  // Stop a running VM. This will release any resources associated with the VM.
  bool Stop();

 private:
  CrosVM(const std::string& name,
         const base::FilePath& vm_kernel,
         const base::FilePath& vm_rootfs,
         const base::FilePath& instance_runtime_dir_,
         std::unique_ptr<MacAddress> mac_addr,
         std::shared_ptr<Subnet> subnet,
         std::unique_ptr<VsockCid> cid,
         std::unique_ptr<NfsExport> nfs_export,
         bool release_on_destruction);

  // Save state to the VM instance runtime directory, including the VM pid.
  bool SaveProcessState();
  // Restore any state associated with the VM process.
  bool RestoreProcessState();

  // Build the command line for a crosvm process to run.
  bool BuildCrosVMCommandLine(const base::FilePath& container_disk,
                              bool rw_container);

  // Initialize a running VM. This includes network setup, starting sshd if
  // |ssh| is set, and starting a container if |run_container| is set. The
  // container disk will be mounted as writable only if |rw_container| is set.
  bool VMInit(bool ssh, bool run_container, bool rw_container);

  // Set to true to kill the VM and free all resources when CrosVM is
  // destructed. If false, the VM and its resources will be left alive.
  void SetReleaseOnDestruction(bool release_on_destruction);

  // Teardown this CrosVM instance. If this instance is not set to release on
  // destruction, this will leave the VM and its resource intact. Otherwise,
  // this will shut down the VM and free all its resources.
  bool Teardown();

  // Stops a crosvm instance using its control socket.
  bool StopCrosvm();

  // RPC calls to maitred.
  bool LaunchProcess(const std::vector<std::string>& args,
                     bool respawn,
                     bool wait_for_exit);
  bool Mount(const std::string& source,
             const std::string& target,
             const std::string& fstype,
             const uint64_t mountflags,
             const std::string& options);
  bool ConfigureNetwork();
  bool Shutdown();

  const std::string name_;
  const base::FilePath vm_kernel_;
  const base::FilePath vm_rootfs_;
  const base::FilePath instance_runtime_dir_;

  const std::unique_ptr<MacAddress> mac_addr_;
  const std::shared_ptr<Subnet> subnet_;
  const std::unique_ptr<VsockCid> cid_;
  const std::unique_ptr<NfsExport> nfs_export_;

  bool release_on_destruction_;

  std::unique_ptr<brillo::ProcessImpl> vm_process_;
  std::unique_ptr<vm_tools::Maitred::Stub> stub_;

  DISALLOW_COPY_AND_ASSIGN(CrosVM);
};
}  // namespace launcher
}  // namespace vm_tools

#endif  // VM_TOOLS_LAUNCHER_CROSVM_H_
