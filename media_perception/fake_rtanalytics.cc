// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/fake_rtanalytics.h"

#include <string>
#include <vector>

#include "media_perception/media_perception_mojom.pb.h"
#include "media_perception/proto_mojom_conversion.h"

namespace mri {

std::vector<PerceptionInterfaceType> FakeRtanalytics::SetupConfiguration(
    const std::string& configuration_name,
    SerializedSuccessStatus* success_status) {
  SuccessStatus status;
  status.set_success(true);
  status.set_failure_reason(configuration_name);
  *success_status = SerializeSuccessStatusProto(status);
  std::vector<PerceptionInterfaceType> interface_types;
  interface_types.push_back(PerceptionInterfaceType::INTERFACE_TYPE_UNKNOWN);
  return interface_types;
}

}  // namespace mri
