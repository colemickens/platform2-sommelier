// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a utility to clear internal crypto entropy (if applicable) from
// |BiometricsManager|s, so as to render useless templates and other user data
// encrypted with old secrets.

#include <base/logging.h>
#include <base/message_loop/message_loop.h>

#include "biod/cros_fp_biometrics_manager.h"

int main(int argc, char* argv[]) {
  base::MessageLoopForIO message_loop;
  std::vector<std::unique_ptr<biod::BiometricsManager>> managers_;

  // Add all the possible BiometricsManagers available.
  std::unique_ptr<biod::BiometricsManager> cros_fp_bio =
      biod::CrosFpBiometricsManager::Create();
  if (cros_fp_bio) {
    managers_.emplace_back(std::move(cros_fp_bio));
  }

  int ret = 0;
  for (const auto& biometrics_manager : managers_) {
    if (!biometrics_manager->ResetEntropy()) {
      LOG(ERROR) << "Failed to reset entropy for sensor type: "
                 << biometrics_manager->GetType();
      ret = -1;
    }
  }

  return ret;
}
