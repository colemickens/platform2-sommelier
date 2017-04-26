// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HAMMERD_HAMMER_UPDATER_H_
#define HAMMERD_HAMMER_UPDATER_H_

#include <string>

#include <base/macros.h>

#include "hammerd/update_fw.h"

namespace hammerd {

class HammerUpdater {
 public:
  explicit HammerUpdater(const std::string& image);
  bool Run();

 private:
  // The main firmware updater.
  FirmwareUpdater fw_updater_;
  // The image data to be updated.
  std::string image_;

  DISALLOW_COPY_AND_ASSIGN(HammerUpdater);
};

}  // namespace hammerd
#endif  // HAMMERD_HAMMER_UPDATER_H_
