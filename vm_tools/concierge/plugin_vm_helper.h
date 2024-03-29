// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_PLUGIN_VM_HELPER_H_
#define VM_TOOLS_CONCIERGE_PLUGIN_VM_HELPER_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>

#include "vm_tools/common/vm_id.h"

namespace vm_tools {
namespace concierge {
namespace pvm {
namespace helper {

// TODO(kimjae): Once fully transitioned to DLC based PluginVM, this check needs
// to be a precondition that's required.
bool IsDlcVm();
bool CreateVm(const VmId& vm_id, std::vector<std::string> params);
bool DeleteVm(const VmId& vm_id);
bool AttachIso(const VmId& vm_id,
               const std::string& cdrom_name,
               const std::string& iso_name);
bool CreateCdromDevice(const VmId& vm_id, const std::string& iso_name);

void CleanUpAfterInstall(const VmId& vm_id, const base::FilePath& iso_path);

}  // namespace helper
}  // namespace pvm
}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_PLUGIN_VM_HELPER_H_
