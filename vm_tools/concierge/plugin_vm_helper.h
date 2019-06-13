// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_PLUGIN_VM_HELPER_H_
#define VM_TOOLS_CONCIERGE_PLUGIN_VM_HELPER_H_

#include <string>
#include <vector>

#include "vm_tools/common/vm_id.h"

namespace vm_tools {
namespace concierge {
namespace pvm {
namespace helper {

bool CreateVm(const VmId& vm_id, std::vector<std::string> params);
bool DeleteVm(const VmId& vm_id);
bool AttachIso(const VmId& vm_id, const std::string& iso_name);

}  // namespace helper
}  // namespace pvm
}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_PLUGIN_VM_HELPER_H_
