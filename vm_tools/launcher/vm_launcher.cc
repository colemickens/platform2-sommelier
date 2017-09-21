// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <base/command_line.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/flag_helper.h>
#include <brillo/process.h>
#include <brillo/syslog_logging.h>

#include "vm_tools/launcher/constants.h"
#include "vm_tools/launcher/crosvm.h"

using std::cerr;
using std::cout;
using std::endl;

namespace {

void Usage(const std::string& program, const std::string& subcommand) {
  if (subcommand == "run" || subcommand == "start") {
    cout << "Usage: " << program << " " << subcommand
         << " VM_NAME ([ --container=PATH ] | [ --rwcontainer=PATH ])\n"
         << "         [ --nfs=PATH ] [ --ssh ] [ --vm_path=PATH ]\n";

    if (subcommand == "run")
      cout << "Run a VM in the foreground with serial console.\n";
    else
      cout << "Start a headless VM. Returns once the VM has booted\n\n";

    cout
        << "Arguments: VM_NAME - An arbitrary name for the VM. Required. Must\n"
        << "                     not be 'all'.\n\n"
        << "Flags: --container=PATH - Optional container disk image to mount.\n"
        << "                          If not specified, the VM will not run a\n"
        << "                          container.\n"
        << "       --rwcontainer=PATH - Same as the --container flag, but the\n"
        << "                            disk image will be mounted read-write\n"
        << "       --nfs=PATH - Optional path to a directory to mount via NFS\n"
        << "       --ssh - Enable ssh in the VM. Only functional on VM test\n"
        << "               images.\n"
        << "       --vm_path=PATH - Optional path to a custom VM\n"
        << "                        kernel/rootfs.\n";

  } else if (subcommand == "stop") {
    cout << "Usage: " << program << " " << subcommand
         << " (VM_NAME | all) [ --force ]\n"
         << "Shut down a VM with the given name.\n\n"
         << "Arguments: (VM_NAME | all) - VM name to stop. 'all' will stop all "
         << "                             running VMs. \n\n";

  } else if (subcommand == "getname") {
    cout << "Usage: " << program << " " << subcommand << " PID\n"
         << "Print the name for a VM with a given PID to stdout.\n\n"
         << "Arguments: PID - PID to find a VM name for. \n";

  } else {
    cout << "Usage: " << program << " run     VM_NAME\n"
         << "       " << program << " start   VM_NAME\n"
         << "       " << program << " stop    (VM_NAME | all)\n"
         << "       " << program << " getname PID\n"
         << "       " << program << " help    SUBCOMMAND\n\n"
         << "Run `" << program
         << " help SUBCOMMAND` for specific usage and flags.\n";
  }
  exit(1);
}

// TODO(smbarber): This assumes there is only one component version loaded at a
// time. This should handle component upgrades and load the latest version
// that's available. http://crbug.com/769625
base::FilePath GetLatestVMPath() {
  base::FilePath component_dir(vm_tools::launcher::kVmDefaultPath);
  base::FileEnumerator dir_enum(component_dir, false,
                                base::FileEnumerator::DIRECTORIES);

  return dir_enum.Next();
}

}  // namespace

int main(int argc, char** argv) {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  auto args = cl->GetArgs();

  if (args.size() < 2)
    Usage(cl->GetProgram().value(), "");
  else if (cl->HasSwitch("help"))
    Usage(cl->GetProgram().value(), args[1]);

  std::string subcommand = args[0];

  // TODO(smbarber): Make an init script do this.
  int rc = mkdir(vm_tools::launcher::kVmRuntimeDirectory, 0700);
  if (rc && errno != EEXIST) {
    PLOG(ERROR) << "Failed to create vm runtime directory";
    return 1;
  }

  // Handle run and start. Both have the same args, and only differ in
  // I/O and blocking.
  if (subcommand == "run" || subcommand == "start") {
    std::string vm_name = args[1];
    if (vm_name == "all") {
      cout << "'all' is reserved and cannot be used for a VM name\n";
      return 1;
    }

    base::FilePath nfs = cl->GetSwitchValuePath("nfs");

    base::FilePath vm_path = cl->GetSwitchValuePath("vm_path");
    if (vm_path.empty())
      vm_path = GetLatestVMPath();

    base::FilePath container_disk = cl->GetSwitchValuePath("rwcontainer");
    bool rw_container = false;
    if (!container_disk.empty()) {
      rw_container = true;
    } else {
      container_disk = cl->GetSwitchValuePath("container");
    }

    bool ssh = cl->HasSwitch("ssh");

    base::FilePath kernel_path =
        vm_path.Append(vm_tools::launcher::kVmKernelName);
    base::FilePath rootfs_path =
        vm_path.Append(vm_tools::launcher::kVmRootfsName);

    auto crosvm = vm_tools::launcher::CrosVM::Create(vm_name, kernel_path,
                                                     rootfs_path, nfs);
    if (!crosvm)
      return 1;

    if (subcommand == "run") {
      if (!crosvm->Run(ssh, container_disk, rw_container))
        return 1;
    } else if (subcommand == "start") {
      if (!crosvm->Start(ssh, container_disk, rw_container)) {
        cout << "Failed to start VM '" << vm_name << "'\n";
        return 1;
      }
      cout << "VM '" << vm_name << "' started\n";
    }
  } else if (subcommand == "stop") {
    std::string vm_name = args[1];

    if (vm_name == "all") {
      bool all_vms_stopped = true;

      base::FileEnumerator file_enum(
          base::FilePath(vm_tools::launcher::kVmRuntimeDirectory),
          false,  // recursive
          base::FileEnumerator::DIRECTORIES);
      for (base::FilePath instance_dir = file_enum.Next();
           !instance_dir.empty(); instance_dir = file_enum.Next()) {
        std::string vm_name = instance_dir.BaseName().value();
        auto crosvm = vm_tools::launcher::CrosVM::Load(vm_name);
        if (!crosvm)
          return 1;

        if (!crosvm->Stop()) {
          cout << "Failed to stop VM '" << vm_name << "'\n";
          all_vms_stopped = false;
        }
        cout << "VM '" << vm_name << "' stopped\n";
      }
      return !all_vms_stopped;
    }

    auto crosvm = vm_tools::launcher::CrosVM::Load(vm_name);
    if (!crosvm)
      return 1;

    if (!crosvm->Stop()) {
      cout << "Failed to stop VM '" << vm_name << "'\n";
      return 1;
    }
    cout << "VM '" << vm_name << "' stopped\n";
  } else if (subcommand == "getname") {
    std::string pid_raw = args[1];
    pid_t pid;
    if (!base::StringToInt(args[1], &pid)) {
      cerr << "Couldn't parse '" << pid_raw << "' as a pid\n";
      return 1;
    }
    std::string vm_name;
    if (!vm_tools::launcher::CrosVM::GetNameForPid(pid, &vm_name)) {
      cerr << "No VM associated with " << pid << endl;
      return 1;
    }
    cout << vm_name << endl;
  } else {
    Usage(cl->GetProgram().value(), args[1]);
  }

  return 0;
}
