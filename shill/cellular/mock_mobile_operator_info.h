// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_MOBILE_OPERATOR_INFO_H_
#define SHILL_CELLULAR_MOCK_MOBILE_OPERATOR_INFO_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/cellular/mobile_operator_info.h"

namespace shill {

class MockMobileOperatorInfo : public MobileOperatorInfo {
 public:
  MockMobileOperatorInfo(EventDispatcher* dispatcher,
                         const std::string& info_owner);
  ~MockMobileOperatorInfo() override;

  MOCK_CONST_METHOD0(IsMobileNetworkOperatorKnown, bool());

  MOCK_CONST_METHOD0(mccmnc, const std::string&());
  MOCK_CONST_METHOD0(olp_list,
                     const std::vector<MobileOperatorInfo::OnlinePortal>&());
  MOCK_CONST_METHOD0(activation_code, const std::string&());
  MOCK_CONST_METHOD0(operator_name, const std::string&());
  MOCK_CONST_METHOD0(country, const std::string&());
  MOCK_CONST_METHOD0(uuid, const std::string&());

  MOCK_METHOD1(UpdateMCCMNC, void(const std::string&));
  MOCK_METHOD1(UpdateSID, void(const std::string&));
  MOCK_METHOD1(UpdateIMSI, void(const std::string&));
  MOCK_METHOD1(UpdateNID, void(const std::string&));
  MOCK_METHOD1(UpdateOperatorName, void(const std::string&));

 private:
  std::string empty_mccmnc_;
  std::vector<MobileOperatorInfo::OnlinePortal> empty_olp_list_;
  std::string empty_activation_code_;
  std::string empty_operator_name_;
  std::string empty_country_;
  std::string empty_uuid_;
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MOBILE_OPERATOR_INFO_H_
