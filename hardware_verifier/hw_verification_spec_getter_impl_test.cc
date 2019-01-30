/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <memory>
#include <string>

#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include "hardware_verifier/hardware_verifier.pb.h"
#include "hardware_verifier/hw_verification_spec_getter_impl.h"
#include "hardware_verifier/test_utils.h"

using google::protobuf::util::MessageDifferencer;

namespace hardware_verifier {

class HwVerificationSpecGetterImplTest : public testing::Test {
 protected:
  void SetUp() {
    vp_getter_ = std::make_unique<HwVerificationSpecGetterImpl>();
    vp_getter_->root_ = GetTestDataPath();
  }

  void SetFakeRoot(const std::string& root_name) {
    vp_getter_->root_ = GetTestDataPath().Append(root_name);
  }

  void SetCrosDebugFlag(bool value) {
    vp_getter_->check_cros_debug_flag_ = value;
  }

  std::unique_ptr<HwVerificationSpecGetterImpl> vp_getter_;
};

TEST_F(HwVerificationSpecGetterImplTest, TestGetDefaultPass) {
  SetFakeRoot("test_root1");
  HwVerificationSpec expected_vp;
  const auto actual_vp = vp_getter_->GetDefault();
  EXPECT_TRUE(actual_vp);
  EXPECT_TRUE(MessageDifferencer::Equivalent(actual_vp.value(), expected_vp));
}

TEST_F(HwVerificationSpecGetterImplTest, TestGetDefaultFail) {
  // The verification spec file in |test_root2| contains invalid data.
  SetFakeRoot("test_root2");
  EXPECT_FALSE(vp_getter_->GetDefault());
}

TEST_F(HwVerificationSpecGetterImplTest, TestGetFromFileCrosDebugOff) {
  const auto tmp_path = GetTestDataPath().Append("test_root1").Append("tmp");

  // cros_debug=0 in unittest envronment.
  EXPECT_FALSE(vp_getter_->GetFromFile(
      tmp_path.Append("hw_verification_spec1.prototxt")));
}

TEST_F(HwVerificationSpecGetterImplTest, TestGetFromFileNoCheckCrosDebug) {
  SetCrosDebugFlag(false);

  const auto tmp_path = GetTestDataPath().Append("test_root1").Append("tmp");

  HwVerificationSpec expected_vp;
  const auto actual_vp = vp_getter_->GetFromFile(
      tmp_path.Append("hw_verification_spec1.prototxt"));
  EXPECT_TRUE(actual_vp);
  EXPECT_TRUE(MessageDifferencer::Equivalent(actual_vp.value(), expected_vp));

  // |hw_verification_spec2.prototxt| contains invalid data.
  EXPECT_FALSE(vp_getter_->GetFromFile(
      tmp_path.Append("hw_verification_spec2.prototxt")));

  // |hw_verification_spec3.prototxt| doesn't exist.
  EXPECT_FALSE(vp_getter_->GetFromFile(
      tmp_path.Append("hw_verification_spec3.prototxt")));
}

}  // namespace hardware_verifier
