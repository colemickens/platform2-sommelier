// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_SETUP_ANDROID_SDK_VERSION_H_
#define ARC_SETUP_ANDROID_SDK_VERSION_H_

namespace arc {

enum class AndroidSdkVersion {
  UNKNOWN = 0,
  ANDROID_M = 23,
  ANDROID_N_MR1 = 25,
  ANDROID_P = 28,
  // TODO(yusukes): Assign an arbitrary number greater than 28 for now. Change
  // this to the actual number once it's ready.
  ANDROID_Q = ANDROID_P + 1,
};

}  // namespace arc

#endif  // ARC_SETUP_ANDROID_SDK_VERSION_H_
