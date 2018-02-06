// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_UDEV_UTIL_H_
#define POWER_MANAGER_POWERD_SYSTEM_UDEV_UTIL_H_

#include <string>

namespace power_manager {
namespace system {

class UdevInterface;

namespace udev_util {

// Returns the first ancestor which is wake capable (i.e has power/wakeup
// property). If the passed device with sysfs path |syspath| is wake capable,
// returns the same.
// For input devices controlled by 'crosec' which are not wake capable
// by themselves, this function is expected to travel the hierarchy to find
// crosec which is wake capable.
// For USB devices, the input device does not have a power/wakeup property
// itself, but the corresponding USB device does. If the matching device does
// not have a power/wakeup property, we thus fall back to the first ancestor
// that has one. Conflicts should not arise, since real-world USB input devices
// typically only expose one input interface anyway. However, crawling up sysfs
// should only reach the first "usb_device" node, because higher-level nodes
// include USB hubs, and enabling wakeups on those isn't a good idea.
bool FindWakeCapableParent(const std::string& syspath,
                           UdevInterface* udev,
                           std::string* parent_syspath_out);

}  // namespace udev_util
}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_UDEV_UTIL_H_
