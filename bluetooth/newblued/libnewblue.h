// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_LIBNEWBLUE_H_
#define BLUETOOTH_NEWBLUED_LIBNEWBLUE_H_

#include <stdint.h>

#include <newblue/att.h>
#include <newblue/btleHid.h>
#include <newblue/gatt.h>
#include <newblue/gatt-builtin.h>
#include <newblue/hci.h>
#include <newblue/l2cap.h>
#include <newblue/sm.h>

#include <base/macros.h>
#include <cstdint>

#include "bluetooth/newblued/libnewblue_generated_function.h"

namespace bluetooth {

// Interface to libnewblue C functions, methods are 1-to-1 correspondence with
// libnewblue public functions. This provides mockable/stubbable libnewblue.
class LibNewblue {
 public:
  LibNewblue() = default;
  virtual ~LibNewblue() = default;

  // att.h
  LIBNEWBLUE_METHOD0(AttInit, attInit, bool());
  LIBNEWBLUE_METHOD0(AttDeinit, attDeinit, void());

  // gatt.h
  LIBNEWBLUE_METHOD0(GattProfileInit, gattProfileInit, bool());
  LIBNEWBLUE_METHOD0(GattProfileDeinit, gattProfileDeinit, void());
  LIBNEWBLUE_METHOD3(GattClientConnect,
                     gattClientConnect,
                     gatt_client_conn_t(void* userData,
                                        const struct bt_addr*,
                                        gattCliConnectResultCbk));
  LIBNEWBLUE_METHOD1(GattClientDisconnect,
                     gattClientDisconnect,
                     uint8_t(gatt_client_conn_t));
  LIBNEWBLUE_METHOD5(
      GattClientEnumServices,
      gattClientEnumServices,
      uint8_t(void*, gatt_client_conn_t, bool, uniq_t, gattCliSvcEnumCbk));
  LIBNEWBLUE_METHOD5(GattClientUtilFindAndTraversePrimaryService,
                     gattClientUtilFindAndTraversePrimaryService,
                     uint8_t(void*,
                             gatt_client_conn_t,
                             const struct uuid*,
                             uniq_t,
                             gattCliUtilSvcTraversedCbk));

  // gatt-builtin.h
  LIBNEWBLUE_METHOD0(GattBuiltinInit, gattBuiltinInit, bool());
  LIBNEWBLUE_METHOD0(GattBuiltinDeinit, gattBuiltinDeinit, void());

  // hci.h
  LIBNEWBLUE_METHOD3(HciUp,
                     hciUp,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(const uint8_t*, hciReadyForUpCbk, void*));
  LIBNEWBLUE_METHOD0(HciDown, hciDown, void());
  LIBNEWBLUE_METHOD0(HciIsUp, hciIsUp, bool());
  LIBNEWBLUE_METHOD4(HciDiscoverLeStart,
                     hciDiscoverLeStart,
                     uniq_t(hciDeviceDiscoveredLeCbk, void*, bool, bool));
  LIBNEWBLUE_METHOD1(HciDiscoverLeStop,
                     hciDiscoverLeStop,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(uniq_t));
  LIBNEWBLUE_METHOD0(HciAdvIsPowerLevelSettingSupported,
                     hciAdvIsPowerLevelSettingSupported,
                     bool());
  LIBNEWBLUE_METHOD0(HciAdvSetAllocate, hciAdvSetAllocate, hci_adv_set_t());
  LIBNEWBLUE_METHOD1(HciAdvSetFree,
                     hciAdvSetFree,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(hci_adv_set_t));
  LIBNEWBLUE_METHOD4(HciAdvSetConfigureData,
                     hciAdvSetConfigureData,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(hci_adv_set_t, bool, const uint8_t*, uint32_t));
  virtual bool HciAdvSetSetAdvParams(hci_adv_set_t set,
                                     uint16_t advIntervalMin,
                                     uint16_t advIntervalMax,
                                     uint8_t advType,
                                     uint8_t ownAddressType,
                                     struct bt_addr* directAddr,
                                     uint8_t advChannelMap,
                                     uint8_t advFilterPolicy,
                                     int8_t advDesiredTxPowerLevel) {
    return hciAdvSetSetAdvParams(set, advIntervalMin, advIntervalMax, advType,
                                 ownAddressType, directAddr, advChannelMap,
                                 advFilterPolicy, advDesiredTxPowerLevel);
  }
  LIBNEWBLUE_METHOD1(HciAdvSetEnable,
                     hciAdvSetEnable,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(hci_adv_set_t));
  LIBNEWBLUE_METHOD1(HciAdvSetDisable,
                     hciAdvSetDisable,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(hci_adv_set_t));

  // l2cap.h
  LIBNEWBLUE_METHOD0(L2cInit, l2cInit, int());
  LIBNEWBLUE_METHOD0(L2cDeinit, l2cDeinit, void());

  // sm.h
  LIBNEWBLUE_METHOD0(SmInit, smInit, bool());
  LIBNEWBLUE_METHOD0(SmDeinit, smDeinit, void());
  LIBNEWBLUE_METHOD2(SmRegisterPairStateObserver,
                     smRegisterPairStateObserver,
                     uniq_t(void*, smPairStateChangeCbk));
  LIBNEWBLUE_METHOD1(SmUnregisterPairStateObserver,
                     smUnregisterPairStateObserver,
                     void(uniq_t));
  LIBNEWBLUE_METHOD2(SmPair,
                     smPair,
                     void(const struct bt_addr*,
                          const struct smPairSecurityRequirements*));
  LIBNEWBLUE_METHOD1(SmUnpair, smUnpair, void(const struct bt_addr*));
  LIBNEWBLUE_METHOD1(SmStartEncryption,
                     smStartEncryption,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(const struct bt_addr*));
  LIBNEWBLUE_METHOD2(SmRegisterPasskeyDisplayObserver,
                     smRegisterPasskeyDisplayObserver,
                     uniq_t(void*, smPasskeyDisplayCbk));
  LIBNEWBLUE_METHOD0(SmGetKnownDevices,
                     smGetKnownDevices,
                     struct smKnownDevNode*());
  LIBNEWBLUE_METHOD1(SmKnownDevicesFree,
                     smKnownDevicesFree,
                     void(struct smKnownDevNode*));
  LIBNEWBLUE_METHOD2(SmSetBlockedLtks,
                     smSetBlockedLtks,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(const struct smKey*, uint8_t));

  // btleHid.h
  LIBNEWBLUE_METHOD2(BtleHidInit,
                     btleHidInit,
                     void(BtleHidConnStateCbk, BtleHidReportRxCbk));
  LIBNEWBLUE_METHOD1(BtleHidAttach,
                     btleHidAttach,
                     ble_hid_conn_t(gatt_client_conn_t));
  LIBNEWBLUE_METHOD1(BtleHidDetach,
                     btleHidDetach,
                     /* NOLINTNEXTLINE(readability/casting) */
                     bool(ble_hid_conn_t));

 private:
  DISALLOW_COPY_AND_ASSIGN(LibNewblue);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_LIBNEWBLUE_H_
