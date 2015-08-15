// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_MOCK_CONFIG_STORE_H_
#define LIBWEAVE_INCLUDE_WEAVE_MOCK_CONFIG_STORE_H_

#include <string>

#include <gmock/gmock.h>
#include <weave/config_store.h>

using testing::_;
using testing::Return;

namespace weave {
namespace unittests {

class MockConfigStore : public ConfigStore {
 public:
  MockConfigStore() {
    EXPECT_CALL(*this, LoadDefaults(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*this, LoadSettings()).WillRepeatedly(Return(""));
    EXPECT_CALL(*this, SaveSettings(_)).WillRepeatedly(Return());
    EXPECT_CALL(*this, OnSettingsChanged(_)).WillRepeatedly(Return());
  }
  MOCK_METHOD1(LoadDefaults, bool(Settings*));
  MOCK_METHOD0(LoadSettings, std::string());
  MOCK_METHOD1(SaveSettings, void(const std::string&));
  MOCK_METHOD1(OnSettingsChanged, void(const Settings&));
};

}  // namespace unittests
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_MOCK_CONFIG_STORE_H_
