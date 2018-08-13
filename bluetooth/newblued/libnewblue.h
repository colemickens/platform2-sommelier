// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_LIBNEWBLUE_H_
#define BLUETOOTH_NEWBLUED_LIBNEWBLUE_H_

#include <stdint.h>

#include <newblue/att.h>
#include <newblue/gatt.h>
#include <newblue/gatt-builtin.h>
#include <newblue/hci.h>
#include <newblue/l2cap.h>
#include <newblue/sm.h>

#include <base/macros.h>

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

  // gatt-builtin.h
  LIBNEWBLUE_METHOD0(GattBuiltinInit, gattBuiltinInit, bool());
  LIBNEWBLUE_METHOD0(GattBuiltinDeinit, gattBuiltinDeinit, void());

  // hci.h
  LIBNEWBLUE_METHOD3(HciUp,
                     hciUp,
                     bool(const uint8_t*, hciReadyForUpCbk, void*));
  LIBNEWBLUE_METHOD0(HciDown, hciDown, void());
  LIBNEWBLUE_METHOD0(HciIsUp, hciIsUp, bool());
  LIBNEWBLUE_METHOD4(HciDiscoverLeStart,
                     hciDiscoverLeStart,
                     uniq_t(hciDeviceDiscoveredLeCbk, void*, bool, bool));
  LIBNEWBLUE_METHOD1(HciDiscoverLeStop, hciDiscoverLeStop, bool(uniq_t));

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

 private:
  DISALLOW_COPY_AND_ASSIGN(LibNewblue);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_LIBNEWBLUE_H_
