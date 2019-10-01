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
  MOCK_METHOD(bool, AttInit, (), (override));
  MOCK_METHOD(void, AttDeinit, (), (override));

  // gatt.h
  MOCK_METHOD(bool, GattProfileInit, (), (override));
  MOCK_METHOD(void, GattProfileDeinit, (), (override));
  MOCK_METHOD(gatt_client_conn_t,
              GattClientConnect,
              (void* userData,
               const struct bt_addr*,
               const GattConnectParameters*,
               gattCliConnectResultCbk),
              (override));
  MOCK_METHOD(uint8_t, GattClientDisconnect, (gatt_client_conn_t), (override));
  MOCK_METHOD(uint8_t,
              GattClientEnumServices,
              (void*, gatt_client_conn_t, bool, uniq_t, gattCliSvcEnumCbk),
              (override));
  MOCK_METHOD(uint8_t,
              GattClientUtilFindAndTraversePrimaryService,
              (void*,
               gatt_client_conn_t,
               const struct uuid*,
               uniq_t,
               gattCliUtilSvcTraversedCbk),
              (override));
  MOCK_METHOD(uint8_t,
              GattClientUtilLongRead,
              (void*,
               gatt_client_conn_t,
               uint16_t,
               uint8_t,
               uniq_t,
               gattCliUtilLongReadCompletedCbk),
              (override));

  // gatt-builtin.h
  MOCK_METHOD(bool, GattBuiltinInit, (), (override));
  MOCK_METHOD(void, GattBuiltinDeinit, (), (override));

  // hci.h
  MOCK_METHOD(bool,
              HciUp,
              (const uint8_t*, hciReadyForUpCbk, void*),
              (override));
  MOCK_METHOD(void, HciDown, (), (override));
  MOCK_METHOD(bool, HciIsUp, (), (override));
  MOCK_METHOD(uniq_t,
              HciDiscoverLeStart,
              (hciDeviceDiscoveredLeCbk,
               void*,
               bool,
               uint16_t,
               uint16_t,
               bool,
               bool,
               bool),
              (override));
  MOCK_METHOD(bool, HciDiscoverLeStop, (uniq_t), (override));
  MOCK_METHOD(bool, HciAdvSetEnable, (hci_adv_set_t), (override));

  // l2cap.h
  MOCK_METHOD(int, L2cInit, (), (override));
  MOCK_METHOD(void, L2cDeinit, (), (override));

  // sm.h
  MOCK_METHOD(bool, SmInit, (), (override));
  MOCK_METHOD(void, SmDeinit, (), (override));
  MOCK_METHOD(uniq_t,
              SmRegisterPairStateObserver,
              (void*, smPairStateChangeCbk),
              (override));
  MOCK_METHOD(void, SmUnregisterPairStateObserver, (uniq_t), (override));
  MOCK_METHOD(void,
              SmPair,
              (const struct bt_addr*, const struct smPairSecurityRequirements*),
              (override));
  MOCK_METHOD(void, SmUnpair, (const struct bt_addr*), (override));
  MOCK_METHOD(uniq_t,
              SmRegisterPasskeyDisplayObserver,
              (void*, smPasskeyDisplayCbk),
              (override));
  MOCK_METHOD(struct smKnownDevNode*, SmGetKnownDevices, (), (override));
  MOCK_METHOD(void, SmKnownDevicesFree, (struct smKnownDevNode*), (override));

  // btleHid.h
  MOCK_METHOD(void,
              BtleHidInit,
              (BtleHidConnStateCbk, BtleHidReportRxCbk),
              (override));
  MOCK_METHOD(ble_hid_conn_t,
              BtleHidAttach,
              (gatt_client_conn_t, const char*),
              (override));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_MOCK_LIBNEWBLUE_H_
