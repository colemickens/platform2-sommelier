// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_mobile_operator_info.h"

#include <gmock/gmock.h>

namespace shill {

MockMobileOperatorInfo::MockMobileOperatorInfo(EventDispatcher* dispatcher,
                                               const std::string& info_owner)
    : MobileOperatorInfo(dispatcher, info_owner) {}

MockMobileOperatorInfo::~MockMobileOperatorInfo() {}

void MockMobileOperatorInfo::SetEmptyDefaultsForProperties() {
  ON_CALL(*this, mccmnc()).WillByDefault(ReturnRef(empty_mccmnc_));
  ON_CALL(*this, olp_list()).WillByDefault(ReturnRef(empty_olp_list_));
  ON_CALL(*this, activation_code())
      .WillByDefault(ReturnRef(empty_activation_code_));
  ON_CALL(*this, operator_name())
      .WillByDefault(ReturnRef(empty_operator_name_));
  ON_CALL(*this, country())
      .WillByDefault(ReturnRef(empty_country_));
  ON_CALL(*this, uuid()).WillByDefault(ReturnRef(empty_uuid_));
}

}  // namespace shill
