// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_PUMA_FW_CTRL_H_
#define CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_PUMA_FW_CTRL_H_

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>

namespace bizlink_updater {

bool GetFwBinInfo(const base::FilePath& fw_path, uint32_t* version);
bool VerifyFwBin(const base::ScopedFD& fd);

}  // namespace bizlink_updater

#endif  // CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_PUMA_FW_CTRL_H_
