// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Standard C++ Library.
#include <utility>

// Chrome OS Library.
#include <base/logging.h>

// Chrome OS Testing Library.
#include <gtest/gtest.h>

// Portier Library.
#include "portier/status.h"

namespace portier {

using std::string;

using Code = Status::Code;

// Test data.
namespace {

const char kEmptyString[] = "";

const int kBadCodeValue = 120;
const char kExpectedBadCodeName[] = "Unknown (120)";

const char kMessage1Part1[] = "Output parameter `";
const char kMessage1Variable[] = "ll_address";
const char kMessage1Part2[] = "' must not be null";
Code kCode1 = Status::Code::INVALID_ARGUMENT;
const char kExpectedToString1[] =
    "Invalid Argument: Output parameter `ll_address' must not be null";

const char kSubMessage2[] = "Require CAP_NET_RAW to open raw socket";
const char kMessage2[] = "Failed to open socket";
const Code kCode2 = Status::Code::BAD_PERMISSIONS;
const char kExpectedToString2[] =
    "Bad Permissions: Failed to open socket: Require CAP_NET_RAW to open raw "
    "socket";

const char kSubSubMessage3[] = "Bad checksum";
const char kSubMessage3[] = "Packet validation failed";
const char kMessage3[] = "Could not receive ether frame";
const Code kCode3 = Status::Code::MALFORMED_PACKET;
const char kExpectedToString3[] =
    "Malformed Packet: Could not receive ether frame: Packet validation "
    "failed: Bad checksum";

const char kSubSubMessage4[] = "Failed to set MULTICAST flag";
const char kMessage4[] = "Failed to open socket";
const Code kCode4 = Status::Code::UNEXPECTED_FAILURE;
const char kExpectedToString4[] =
    "Unexpected Failure: Failed to open socket: Failed to set MULTICAST flag";

const char kMessage5NotUsed[] = "Should not appear";
const char kMessage5Expected[] = "Expected message";
const Code kCode5 = Code::BAD_INTERNAL_STATE;
const char kExpectedToString5[] = "Bad Internal State: Expected message";

const char kMessage6[] = "Outgoing packet is larger than MTU size";
const Code kCode6 = Code::MTU_EXCEEDED;
const char kExpectedToString6[] =
    "MTU Exceeded: Outgoing packet is larger than MTU size";

const Code kBadCodes[] = {Code::BAD_PERMISSIONS,    Code::DOES_NOT_EXIST,
                          Code::RESULT_UNAVAILABLE, Code::UNEXPECTED_FAILURE,
                          Code::INVALID_ARGUMENT,   Code::MTU_EXCEEDED,
                          Code::MALFORMED_PACKET,   Code::RESOURCE_IN_USE,
                          Code::UNSUPPORTED_TYPE,   Code::BAD_INTERNAL_STATE};

const size_t kBadCodesLength = sizeof(kBadCodes) / sizeof(Code);

}  // namespace

TEST(StatusTest, EmptyInstance) {
  const Status status;
  EXPECT_EQ(status.code(), Code::OK);
  EXPECT_TRUE(status);
  EXPECT_EQ(status.ToString(), "OK");

  // A compile time check that the ostream insertion operator works.
  LOG(INFO) << status;
}

TEST(StatusTest, VariousCodes) {
  for (int i = 0; i < kBadCodesLength; i++) {
    const Code code = kBadCodes[i];
    const Status status(code);

    EXPECT_EQ(status.code(), code);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.ToString(), Status::GetCodeName(code));
  }
}

TEST(StatusTest, UnknownCode) {
  const Code bad_code = static_cast<Code>(kBadCodeValue);
  const string code_name = Status::GetCodeName(bad_code);
  EXPECT_EQ(code_name, kExpectedBadCodeName);
}

TEST(StatusTest, EmptyStringNoEffect) {
  Status status;
  status << kEmptyString;
  EXPECT_EQ(status.code(), Code::OK);
  EXPECT_TRUE(status);
  EXPECT_EQ(status.ToString(), "OK");
}

// Test having a message constructed across multiple insertions.
TEST(StatusTest, ConstructedMessage) {
  Status status(kCode1);
  status << kMessage1Part1 << kMessage1Variable << kMessage1Part2;

  EXPECT_EQ(status.code(), kCode1);
  EXPECT_FALSE(status);
  EXPECT_EQ(status.ToString(), kExpectedToString1);
}

// Test having a message extending a sub-message.
TEST(StatusTest, SubMessage) {
  // Sub status.
  Status sub_status(kCode2, kSubMessage2);

  // Status.
  Status status(std::move(sub_status));
  status << kMessage2;

  EXPECT_EQ(status.code(), kCode2);
  EXPECT_FALSE(status);
  EXPECT_EQ(status.ToString(), kExpectedToString2);
}

// Test having 3 levels of status message extension.
TEST(StatusTest, SubSubMessage) {
  // Sub sub status.
  Status sub_sub_status(kCode3, kSubSubMessage3);

  // Sub status.
  Status sub_status(std::move(sub_sub_status));
  sub_status << kSubMessage3;

  // Status.
  Status status(std::move(sub_status));
  status << kMessage3;

  EXPECT_EQ(status.code(), kCode3);
  EXPECT_FALSE(status);
  EXPECT_EQ(status.ToString(), kExpectedToString3);
}

// Test having 3 layers of status, but not extending on the second.
TEST(StatusTest, SubSubMessageWithoutSubMessage) {
  // Sub sub status.
  Status sub_sub_status(kCode4, kSubSubMessage4);

  Status sub_status(std::move(sub_sub_status));

  Status status(std::move(sub_status));
  status << kMessage4;

  EXPECT_EQ(status.code(), kCode4);
  EXPECT_FALSE(status);
  EXPECT_EQ(status.ToString(), kExpectedToString4);
}

namespace {
Status ReturnOnFailureTestMethod() {
  Status ok_status;
  PORTIER_RETURN_ON_FAILURE(ok_status) << kMessage5NotUsed;
  Status bad_status(kCode5);
  PORTIER_RETURN_ON_FAILURE(bad_status) << kMessage5Expected;
  return Status();
}

Status ReturnOnConstruction() {
  return Status(kCode6) << kMessage6;
}
}  // namespace

TEST(StatusTest, ReturnOnFailureMacroTest) {
  const Status status = ReturnOnFailureTestMethod();

  EXPECT_EQ(status.code(), kCode5);
  EXPECT_FALSE(status);
  EXPECT_EQ(status.ToString(), kExpectedToString5);
}

TEST(StatusTest, ReturnOnConstructionTest) {
  const Status status = ReturnOnConstruction();

  EXPECT_EQ(status.code(), kCode6);
  EXPECT_FALSE(status);
  EXPECT_EQ(status.ToString(), kExpectedToString6);
}

}  // namespace portier
