// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CECSERVICE_CEC_FD_MOCK_H_
#define CECSERVICE_CEC_FD_MOCK_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "cecservice/cec_fd.h"

namespace cecservice {

class CecFdMock : public CecFd {
 public:
  CecFdMock() = default;
  ~CecFdMock() override { CecFdDestructorCalled(); }

  MOCK_CONST_METHOD1(SetLogicalAddresses, bool(struct cec_log_addrs*));
  MOCK_CONST_METHOD1(GetLogicalAddresses, bool(struct cec_log_addrs*));
  MOCK_CONST_METHOD1(ReceiveMessage, bool(struct cec_msg*));
  MOCK_CONST_METHOD1(ReceiveEvent, bool(struct cec_event*));
  MOCK_CONST_METHOD1(TransmitMessage, TransmitResult(struct cec_msg*));
  MOCK_CONST_METHOD1(GetCapabilities, bool(struct cec_caps*));
  MOCK_CONST_METHOD1(SetMode, bool(uint32_t));
  MOCK_METHOD1(SetEventCallback, bool(const Callback& callback));
  MOCK_METHOD0(WriteWatch, bool());
  MOCK_METHOD0(CecFdDestructorCalled, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(CecFdMock);
};

}  // namespace cecservice

#endif  // CECSERVICE_CEC_FD_MOCK_H_
