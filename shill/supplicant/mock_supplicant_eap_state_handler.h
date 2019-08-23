// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SUPPLICANT_MOCK_SUPPLICANT_EAP_STATE_HANDLER_H_
#define SHILL_SUPPLICANT_MOCK_SUPPLICANT_EAP_STATE_HANDLER_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/supplicant/supplicant_eap_state_handler.h"

namespace shill {

class MockSupplicantEAPStateHandler : public SupplicantEAPStateHandler {
 public:
  MockSupplicantEAPStateHandler();
  ~MockSupplicantEAPStateHandler() override;

  MOCK_METHOD3(ParseStatus,
               bool(const std::string& status,
                    const std::string& parameter,
                    Service::ConnectFailure* failure));
  MOCK_METHOD0(Reset, void());
  MOCK_CONST_METHOD0(is_eap_in_progress, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSupplicantEAPStateHandler);
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_MOCK_SUPPLICANT_EAP_STATE_HANDLER_H_
