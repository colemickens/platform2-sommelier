// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DPSL_TEST_UTILS_COMMON_H_
#define DIAGNOSTICS_DPSL_TEST_UTILS_COMMON_H_

#include <memory>
#include <string>

#include <google/protobuf/message.h>

namespace diagnostics {
namespace test_utils {

// Prints a proto to std::cout as JSON, including the proto name and the body.
// Returns whether printing was successful.
//
// For message
// message GetOsVersionResponse {
//    string version = "12440.0.2019_08_20_1256"
// }
// The following is printed:
// {
//    "body": {
//       "version": "12440.0.2019_08_20_1256"
//    },
//    "name": "GetOsVersionResponse"
// }
// This format was chosen so that it could be deserialized back to a proto.
bool PrintProto(const google::protobuf::Message& message);

}  // namespace test_utils
}  // namespace diagnostics

#endif  // DIAGNOSTICS_DPSL_TEST_UTILS_COMMON_H_
