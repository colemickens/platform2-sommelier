// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_PROTOBUF_TEST_UTILS_H_
#define CRYPTOHOME_PROTOBUF_TEST_UTILS_H_

#include <string>

#include <gmock/gmock.h>

namespace cryptohome {

// gmock matcher for protobufs, allowing to check protobuf arguments in mocks.
MATCHER_P(ProtobufEquals, expected_message, "") {
  std::string arg_dumped;
  arg.SerializeToString(&arg_dumped);
  std::string expected_message_dumped;
  expected_message.SerializeToString(&expected_message_dumped);
  return arg_dumped == expected_message_dumped;
}

}  // namespace cryptohome

#endif  // CRYPTOHOME_PROTOBUF_TEST_UTILS_H_
