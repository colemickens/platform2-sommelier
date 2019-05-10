// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_VMPLUGIN_DISPATCHER_INTERFACE_H_
#define VM_TOOLS_CONCIERGE_VMPLUGIN_DISPATCHER_INTERFACE_H_

#include <string>

#include <base/files/file_path.h>
#include <dbus/exported_object.h>
#include <dbus/object_proxy.h>

#include <vm_tools/common/vm_id.h>

namespace vm_tools {
namespace concierge {

dbus::ObjectProxy* GetVmpluginServiceProxy(scoped_refptr<dbus::Bus> bus);

bool VmpluginRegisterVm(dbus::ObjectProxy* proxy,
                        const std::string& owner_id,
                        const base::FilePath& disk_image_path);
bool VmpluginUnregisterVm(dbus::ObjectProxy* proxy, const VmId& vm_id);

bool VmpluginIsVmRegistered(dbus::ObjectProxy* proxy,
                            const VmId& vm_id,
                            bool* result);

}  // namespace concierge
}  // namespace vm_tools

#endif  //  VM_TOOLS_CONCIERGE_VMPLUGIN_DISPATCHER_INTERFACE_H_
