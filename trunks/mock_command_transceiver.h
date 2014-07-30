// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_MOCK_COMMAND_TRANSCEIVER_H_
#define TRUNKS_MOCK_COMMAND_TRANSCEIVER_H_

#include <string>

#include <base/basictypes.h>
#include <base/callback.h>
#include <gmock/gmock.h>

#include "trunks/command_transceiver.h"

namespace trunks {

class MockCommandTransceiver : public CommandTransceiver {
 public:
  MockCommandTransceiver();
  virtual ~MockCommandTransceiver();

  MOCK_METHOD2(SendCommand, void(
      const std::string&,
      const base::Callback<void(const std::string& response)>&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCommandTransceiver);
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_COMMAND_TRANSCEIVER_H_
