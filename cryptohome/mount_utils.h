// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOUNT_UTILS_H_
#define CRYPTOHOME_MOUNT_UTILS_H_

#include <google/protobuf/message_lite.h>

#include "cryptohome/platform.h"

namespace cryptohome {

// Cryptohome uses protobufs to communicate with the out-of-process mount
// helper.
bool ReadProtobuf(int fd, google::protobuf::MessageLite* message);
bool WriteProtobuf(int fd, const google::protobuf::MessageLite& message);

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOUNT_UTILS_H_
