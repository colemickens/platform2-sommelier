// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_MESSAGES_H_
#define MIDIS_MESSAGES_H_

#ifdef __cplusplus
extern "C" {
#endif

static const size_t kMidisDeviceInfoNameSize = 80;
static const uint8_t kMidisMaxDevices = 10;

enum ClientMsgType {
  REQUEST_LIST_DEVICES = 0,
};

enum ServerMsgType {
  LIST_DEVICES_RESPONSE = 0,
  DEVICE_ADDED = 1,
  DEVICE_REMOVED = 2,
};

struct MidisMessageHeader {
  uint32_t type;
  uint32_t payload_size;
} __attribute__((packed));

struct MidisDeviceInfo {
  uint32_t card;
  uint32_t device_num;
  uint32_t num_subdevices;
  uint32_t flags;
  uint8_t name[kMidisDeviceInfoNameSize];
} __attribute__((packed));

#ifdef __cplusplus
}
#endif

#endif  // MIDIS_MESSAGES_H_
