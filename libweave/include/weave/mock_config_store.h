// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_MOCK_CONFIG_STORE_H_
#define LIBWEAVE_INCLUDE_WEAVE_MOCK_CONFIG_STORE_H_

#include <map>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <weave/config_store.h>

namespace weave {
namespace unittests {

class MockConfigStore : public ConfigStore {
 public:
  MockConfigStore() {
    using testing::_;
    using testing::Return;

    EXPECT_CALL(*this, LoadDefaults(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*this, LoadSettings()).WillRepeatedly(Return(""));
    EXPECT_CALL(*this, SaveSettings(_)).WillRepeatedly(Return());
    EXPECT_CALL(*this, OnSettingsChanged(_)).WillRepeatedly(Return());
  }
  MOCK_METHOD1(LoadDefaults, bool(Settings*));
  MOCK_METHOD0(LoadSettings, std::string());
  MOCK_METHOD1(SaveSettings, void(const std::string&));
  MOCK_METHOD1(OnSettingsChanged, void(const Settings&));

  MOCK_METHOD0(LoadBaseCommandDefs, std::string());
  MOCK_METHOD0(LoadCommandDefs, std::map<std::string, std::string>());

  MOCK_METHOD0(LoadBaseStateDefs, std::string());
  MOCK_METHOD0(LoadBaseStateDefaults, std::string());

  MOCK_METHOD0(LoadStateDefs, std::map<std::string, std::string>());
  MOCK_METHOD0(LoadStateDefaults, std::vector<std::string>());
};

}  // namespace unittests
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_MOCK_CONFIG_STORE_H_
