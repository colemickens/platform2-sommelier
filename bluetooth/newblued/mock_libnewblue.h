// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_MOCK_LIBNEWBLUE_H_
#define BLUETOOTH_NEWBLUED_MOCK_LIBNEWBLUE_H_

#include <gmock/gmock.h>

#include "bluetooth/newblued/libnewblue.h"

namespace bluetooth {

class MockLibNewblue : public LibNewblue {
 public:
  using LibNewblue::LibNewblue;

  // att.h
  MOCK_METHOD0(AttInit, bool());
  MOCK_METHOD0(AttDeinit, void());

  // gatt.h
  MOCK_METHOD0(GattProfileInit, bool());
  MOCK_METHOD0(GattProfileDeinit, void());
  MOCK_METHOD3(GattClientConnect,
               gatt_client_conn_t(void* userData,
                                  const struct bt_addr*,
                                  gattCliConnectResultCbk));
  MOCK_METHOD1(GattClientDisconnect, uint8_t(gatt_client_conn_t));
  MOCK_METHOD5(
      GattClientEnumServices,
      uint8_t(void*, gatt_client_conn_t, bool, uniq_t, gattCliSvcEnumCbk));
  MOCK_METHOD5(GattClientUtilFindAndTraversePrimaryService,
               uint8_t(void*,
                       gatt_client_conn_t,
                       const struct uuid*,
                       uniq_t,
                       gattCliUtilSvcTraversedCbk));

  // gatt-builtin.h
  MOCK_METHOD0(GattBuiltinInit, bool());
  MOCK_METHOD0(GattBuiltinDeinit, void());

  // hci.h
  MOCK_METHOD3(HciUp, bool(const uint8_t*, hciReadyForUpCbk, void*));
  MOCK_METHOD0(HciDown, void());
  MOCK_METHOD0(HciIsUp, bool());
  MOCK_METHOD4(HciDiscoverLeStart,
               uniq_t(hciDeviceDiscoveredLeCbk, void*, bool, bool));
  MOCK_METHOD1(HciDiscoverLeStop, bool(uniq_t));
  MOCK_METHOD1(HciAdvSetEnable, bool(hci_adv_set_t));

  // l2cap.h
  MOCK_METHOD0(L2cInit, int());
  MOCK_METHOD0(L2cDeinit, void());

  // sm.h
  MOCK_METHOD0(SmInit, bool());
  MOCK_METHOD0(SmDeinit, void());
  MOCK_METHOD2(SmRegisterPairStateObserver,
               uniq_t(void*, smPairStateChangeCbk));
  MOCK_METHOD1(SmUnregisterPairStateObserver, void(uniq_t));
  MOCK_METHOD2(SmPair,
               void(const struct bt_addr*,
                    const struct smPairSecurityRequirements*));
  MOCK_METHOD1(SmUnpair, void(const struct bt_addr*));
  MOCK_METHOD2(SmRegisterPasskeyDisplayObserver,
               uniq_t(void*, smPasskeyDisplayCbk));
  MOCK_METHOD0(SmGetKnownDevices, struct smKnownDevNode*());
  MOCK_METHOD1(SmKnownDevicesFree, void(struct smKnownDevNode*));

  // btleHid.h
  MOCK_METHOD2(BtleHidInit, void(BtleHidConnStateCbk, BtleHidReportRxCbk));
  MOCK_METHOD2(BtleHidAttach, ble_hid_conn_t(gatt_client_conn_t, const char*));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_MOCK_LIBNEWBLUE_H_
