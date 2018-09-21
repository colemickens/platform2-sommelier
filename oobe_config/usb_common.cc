// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/usb_common.h"

namespace oobe_config {

const char kStatefulDir[] = "/mnt/stateful_partition/";
const char kUnencryptedOobeConfigDir[] = "unencrypted/oobe_auto_config/";
const char kConfigFile[] = "config.json";
const char kDomainFile[] = "enrollment_domain";
const char kKeyFile[] = "validation_key.pub";
const char kDevDiskById[] = "/dev/disk/by-id/";
const char kUsbDevicePathSigFile[] = "usb_device_path.sig";

}  // namespace oobe_config
