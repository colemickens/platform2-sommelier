// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_RTANALYTICS_H_
#define MEDIA_PERCEPTION_RTANALYTICS_H_

// This header needs to be buildable from both Google3 and outside, so it cannot
// rely on Google3 dependencies.

#include <string>
#include <vector>

namespace mri {

// Typdefs for readability. Serialized protos are passed back and forth across
// the boundary between platform2 code and librtanalytics.so.
// Note that this alias is guaranteed to always have this type.
using SerializedSuccessStatus = std::vector<uint8_t>;

enum PerceptionInterfaceType {
  INTERFACE_TYPE_UNKNOWN,
};

class Rtanalytics {
 public:
  virtual ~Rtanalytics() = default;

  // Asks the library to setup a particular configuration. Success status is
  // filled in by the library side. The return value is the current list of
  // perception interface types that are fulfilled by the current configuration
  // set. This function can be called multiple times to setup multiple
  // configurations. |success_status| cannot be null.
  virtual std::vector<PerceptionInterfaceType> SetupConfiguration(
      const std::string& configuration_name,
      SerializedSuccessStatus* success_status) = 0;
};

}  // namespace mri

#endif  // MEDIA_PERCEPTION_RTANALYTICS_H_
