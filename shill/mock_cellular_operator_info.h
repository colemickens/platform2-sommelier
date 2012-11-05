// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CELLULAR_OPERATOR_INFO_H_
#define SHILL_MOCK_CELLULAR_OPERATOR_INFO_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/cellular_operator_info.h"

namespace shill {

class MockCellularOperatorInfo : public CellularOperatorInfo {
 public:
  MockCellularOperatorInfo();
  virtual ~MockCellularOperatorInfo();

  MOCK_METHOD1(Load, bool(const FilePath &info_file_path));
  MOCK_METHOD2(GetOLP, bool(const std::string &operator_id,
                            CellularService::OLP *olp));
};

}  // namespace shill

#endif  // SHILL_MOCK_CELLULAR_OPERATOR_INFO_H_
