// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MOBILE_OPERATOR_INFO_H_
#define SHILL_MOCK_MOBILE_OPERATOR_INFO_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/mobile_operator_info.h"

using testing::ReturnRef;

namespace shill {

class MockMobileOperatorInfo : public MobileOperatorInfo {
 public:
  explicit MockMobileOperatorInfo(EventDispatcher *dispatcher);
  virtual ~MockMobileOperatorInfo();

  MOCK_CONST_METHOD0(IsMobileNetworkOperatorKnown, bool());

  MOCK_CONST_METHOD0(mccmnc, const std::string &());
  MOCK_CONST_METHOD0(operator_name, const std::string &());
  MOCK_CONST_METHOD0(uuid, const std::string &());

  // Sets up the mock object to return empty strings/vectors etc for all
  // propeties.
  void SetEmptyDefaultsForProperties();

 private:
  std::string empty_mccmnc_;
  std::string empty_operator_name_;
  std::string empty_uuid_;
};

}  // namespace shill

#endif  // SHILL_MOCK_MOBILE_OPERATOR_INFO_H_
