// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_FAKE_RTANALYTICS_H_
#define MEDIA_PERCEPTION_FAKE_RTANALYTICS_H_

#include "media_perception/rtanalytics.h"

#include <string>
#include <vector>

#include "base/macros.h"

namespace mri {

class FakeRtanalytics : public Rtanalytics {
 public:
  FakeRtanalytics() = default;

  // Rtanalytics:
  std::vector<PerceptionInterfaceType> SetupConfiguration(
      const std::string& configuration_name,
      SerializedSuccessStatus* success_status) override;
  SerializedPipelineState GetPipelineState(
      const std::string& configuration_name) const override;
  SerializedPipelineState SetPipelineState(
      const std::string& configuration_name,
      const SerializedPipelineState* desired_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRtanalytics);
};

}  // namespace mri

#endif  // MEDIA_PERCEPTION_FAKE_RTANALYTICS_H_
