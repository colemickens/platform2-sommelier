// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm/libdfu_notification/idfu_notification.h"

#include <string>

#include "cfm/libdfu_notification/dfu_log_notification.h"

std::unique_ptr<IDfuNotification> IDfuNotification::For(
    const std::string& device_name) {
  return std::make_unique<DfuLogNotification>(device_name);
}
