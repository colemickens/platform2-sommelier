// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_modem_gsm_card_proxy.h"

#include "shill/testing.h"

using testing::_;

namespace shill {

MockModemGSMCardProxy::MockModemGSMCardProxy() {
  ON_CALL(*this, GetIMEI(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, GetIMSI(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, GetSPN(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, GetMSISDN(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, EnablePIN(_, _, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<2>());
  ON_CALL(*this, SendPIN(_, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<1>());
  ON_CALL(*this, SendPUK(_, _, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<2>());
  ON_CALL(*this, ChangePIN(_, _, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<2>());
}

MockModemGSMCardProxy::~MockModemGSMCardProxy() {}

}  // namespace shill
