// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_THROTTLER_H_
#define SHILL_MOCK_THROTTLER_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/throttler.h"

namespace shill {

class MockThrottler : public Throttler {
 public:
  MockThrottler();
  ~MockThrottler() override;

  MOCK_METHOD1(DisableThrottlingOnAllInterfaces,
               bool(const ResultCallback& callback));
  MOCK_METHOD3(ThrottleInterfaces,
               bool(const ResultCallback& callback,
                    uint32_t upload_rate_kbits,
                    uint32_t download_rate_kbits));
  MOCK_METHOD1(ApplyThrottleToNewInterface,
               bool(const std::string& interface_name));

  MOCK_METHOD1(StartTCForCommands,
               bool(const std::vector<std::string>& commands));

  MOCK_METHOD4(Throttle,
               bool(const ResultCallback& callback,
                    const std::string& interface_name,
                    uint32_t upload_rate_kbits,
                    uint32_t download_rate_kbits));

  MOCK_METHOD1(WriteTCCommands, void(int fd));
  MOCK_METHOD1(OnProcessExited, void(int exit_status));

  MOCK_METHOD3(Done,
               void(const ResultCallback& callback,
                    Error::Type error_type,
                    const std::string& message));

  MOCK_METHOD0(GetNextInterface, std::string());

  MOCK_METHOD0(ClearTCState, void());
  MOCK_METHOD0(ClearThrottleStatus, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockThrottler);
};

}  // namespace shill

#endif  // SHILL_MOCK_THROTTLER_H_
