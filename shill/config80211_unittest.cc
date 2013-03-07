// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides tests for individual messages.  It tests
// NetlinkMessageFactory's ability to create specific message types and it
// tests the various NetlinkMessage types' ability to parse those
// messages.

// This file tests the public interface to Config80211.

#include "shill/config80211.h"

#include <netlink/netlink.h>

#include <string>
#include <vector>

#include <base/stl_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_netlink_socket.h"
#include "shill/netlink_attribute.h"
#include "shill/nl80211_message.h"
#include "shill/refptr_types.h"

using base::Bind;
using base::Unretained;
using base::WeakPtr;
using std::string;
using std::vector;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::Test;

namespace shill {

namespace {

// The group_id of the scan and mlme groups are, for the purposes of this
// test, arbitrary as long as they're different.  The values, here, have been
// extracted from a specific run on hardware but that was, in reality, just
// being pedantic.
const uint32_t kEventTypeScanId = 4;
const uint32_t kEventTypeMlmeId = 6;

// These data blocks have been collected by shill using Config80211 while,
// simultaneously (and manually) comparing shill output with that of the 'iw'
// code from which it was derived.  The test strings represent the raw packet
// data coming from the kernel.  The comments above each of these strings is
// the markup that "iw" outputs for ech of these packets.

// These constants are consistent throughout the packets, below.

const uint32_t kExpectedIfIndex = 4;
const uint32_t kWiPhy = 0;
const uint16_t kNl80211FamilyId = 0x13;
const char kExpectedMacAddress[] = "c0:3f:0e:77:e8:7f";

const uint8_t kMacAddressBytes[] = {
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f
};

const uint8_t kAssignedRespIeBytes[] = {
  0x01, 0x08, 0x82, 0x84,
  0x8b, 0x96, 0x0c, 0x12,
  0x18, 0x24, 0x32, 0x04,
  0x30, 0x48, 0x60, 0x6c
};


// wlan0 (phy #0): scan started

const uint32_t kScanFrequencyTrigger[] = {
  2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447,
  2452, 2457, 2462, 2467, 2472, 2484, 5180, 5200,
  5220, 5240, 5260, 5280, 5300, 5320, 5500, 5520,
  5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680,
  5700, 5745, 5765, 5785, 5805, 5825
};

const unsigned char kNL80211_CMD_TRIGGER_SCAN[] = {
  0x68, 0x01, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x21, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x2d, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x34, 0x01, 0x2c, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x6c, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x01, 0x00, 0x71, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x02, 0x00, 0x76, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x03, 0x00, 0x7b, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x04, 0x00, 0x80, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x05, 0x00, 0x85, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x06, 0x00, 0x8a, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x07, 0x00, 0x8f, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x08, 0x00, 0x94, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x09, 0x00, 0x99, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0a, 0x00, 0x9e, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0b, 0x00, 0xa3, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0c, 0x00, 0xa8, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0d, 0x00, 0xb4, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0e, 0x00, 0x3c, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x0f, 0x00, 0x50, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x10, 0x00, 0x64, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x11, 0x00, 0x78, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x12, 0x00, 0x8c, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x13, 0x00, 0xa0, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x14, 0x00, 0xb4, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x15, 0x00, 0xc8, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x16, 0x00, 0x7c, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x17, 0x00, 0x90, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x18, 0x00, 0xa4, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x19, 0x00, 0xb8, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x1a, 0x00, 0xcc, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x1b, 0x00, 0xe0, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x1c, 0x00, 0xf4, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x1d, 0x00, 0x08, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x1e, 0x00, 0x1c, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x1f, 0x00, 0x30, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x20, 0x00, 0x44, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x21, 0x00, 0x71, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x22, 0x00, 0x85, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x23, 0x00, 0x99, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x24, 0x00, 0xad, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x25, 0x00, 0xc1, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00,
};


// wlan0 (phy #0): scan finished: 2412 2417 2422 2427 2432 2437 2442 2447 2452
// 2457 2462 2467 2472 2484 5180 5200 5220 5240 5260 5280 5300 5320 5500 5520
// 5540 5560 5580 5600 5620 5640 5660 5680 5700 5745 5765 5785 5805 5825, ""

const uint32_t kScanFrequencyResults[] = {
  2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447,
  2452, 2457, 2462, 2467, 2472, 2484, 5180, 5200,
  5220, 5240, 5260, 5280, 5300, 5320, 5500, 5520,
  5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680,
  5700, 5745, 5765, 5785, 5805, 5825
};

const unsigned char kNL80211_CMD_NEW_SCAN_RESULTS[] = {
  0x68, 0x01, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x22, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x2d, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x34, 0x01, 0x2c, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x6c, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x01, 0x00, 0x71, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x02, 0x00, 0x76, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x03, 0x00, 0x7b, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x04, 0x00, 0x80, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x05, 0x00, 0x85, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x06, 0x00, 0x8a, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x07, 0x00, 0x8f, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x08, 0x00, 0x94, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x09, 0x00, 0x99, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0a, 0x00, 0x9e, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0b, 0x00, 0xa3, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0c, 0x00, 0xa8, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0d, 0x00, 0xb4, 0x09, 0x00, 0x00,
  0x08, 0x00, 0x0e, 0x00, 0x3c, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x0f, 0x00, 0x50, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x10, 0x00, 0x64, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x11, 0x00, 0x78, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x12, 0x00, 0x8c, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x13, 0x00, 0xa0, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x14, 0x00, 0xb4, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x15, 0x00, 0xc8, 0x14, 0x00, 0x00,
  0x08, 0x00, 0x16, 0x00, 0x7c, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x17, 0x00, 0x90, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x18, 0x00, 0xa4, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x19, 0x00, 0xb8, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x1a, 0x00, 0xcc, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x1b, 0x00, 0xe0, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x1c, 0x00, 0xf4, 0x15, 0x00, 0x00,
  0x08, 0x00, 0x1d, 0x00, 0x08, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x1e, 0x00, 0x1c, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x1f, 0x00, 0x30, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x20, 0x00, 0x44, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x21, 0x00, 0x71, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x22, 0x00, 0x85, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x23, 0x00, 0x99, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x24, 0x00, 0xad, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x25, 0x00, 0xc1, 0x16, 0x00, 0x00,
  0x08, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00,
};


// wlan0: new station c0:3f:0e:77:e8:7f

const uint32_t kNewStationExpectedGeneration = 275;

const unsigned char kNL80211_CMD_NEW_STATION[] = {
  0x34, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x13, 0x01, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x06, 0x00,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x00, 0x00,
  0x08, 0x00, 0x2e, 0x00, 0x13, 0x01, 0x00, 0x00,
  0x04, 0x00, 0x15, 0x00,
};


// wlan0 (phy #0): auth c0:3f:0e:77:e8:7f -> 48:5d:60:77:2d:cf status: 0:
// Successful [frame: b0 00 3a 01 48 5d 60 77 2d cf c0 3f 0e 77 e8 7f c0
// 3f 0e 77 e8 7f 30 07 00 00 02 00 00 00]

const unsigned char kAuthenticateFrame[] = {
  0xb0, 0x00, 0x3a, 0x01, 0x48, 0x5d, 0x60, 0x77,
  0x2d, 0xcf, 0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x30, 0x07,
  0x00, 0x00, 0x02, 0x00, 0x00, 0x00
};

const unsigned char kNL80211_CMD_AUTHENTICATE[] = {
  0x48, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x25, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x22, 0x00, 0x33, 0x00,
  0xb0, 0x00, 0x3a, 0x01, 0x48, 0x5d, 0x60, 0x77,
  0x2d, 0xcf, 0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x30, 0x07,
  0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
};


// wlan0 (phy #0): assoc c0:3f:0e:77:e8:7f -> 48:5d:60:77:2d:cf status: 0:
// Successful [frame: 10 00 3a 01 48 5d 60 77 2d cf c0 3f 0e 77 e8 7f c0 3f 0e
// 77 e8 7f 40 07 01 04 00 00 01 c0 01 08 82 84 8b 96 0c 12 18 24 32 04 30 48
// 60 6c]

const unsigned char kAssociateFrame[] = {
  0x10, 0x00, 0x3a, 0x01, 0x48, 0x5d, 0x60, 0x77,
  0x2d, 0xcf, 0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x40, 0x07,
  0x01, 0x04, 0x00, 0x00, 0x01, 0xc0, 0x01, 0x08,
  0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,
  0x32, 0x04, 0x30, 0x48, 0x60, 0x6c
};

const unsigned char kNL80211_CMD_ASSOCIATE[] = {
  0x58, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x26, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x32, 0x00, 0x33, 0x00,
  0x10, 0x00, 0x3a, 0x01, 0x48, 0x5d, 0x60, 0x77,
  0x2d, 0xcf, 0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x40, 0x07,
  0x01, 0x04, 0x00, 0x00, 0x01, 0xc0, 0x01, 0x08,
  0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,
  0x32, 0x04, 0x30, 0x48, 0x60, 0x6c, 0x00, 0x00,
};


// wlan0 (phy #0): connected to c0:3f:0e:77:e8:7f

const uint16_t kExpectedConnectStatus = 0;

const unsigned char kNL80211_CMD_CONNECT[] = {
  0x4c, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x2e, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x06, 0x00,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x00, 0x00,
  0x06, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x14, 0x00, 0x4e, 0x00, 0x01, 0x08, 0x82, 0x84,
  0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x32, 0x04,
  0x30, 0x48, 0x60, 0x6c,
};


// wlan0 (phy #0): deauth c0:3f:0e:77:e8:7f -> ff:ff:ff:ff:ff:ff reason 2:
// Previous authentication no longer valid [frame: c0 00 00 00 ff ff ff ff
// ff ff c0 3f 0e 77 e8 7f c0 3f 0e 77 e8 7f c0 0e 02 00]

const unsigned char kDeauthenticateFrame[] = {
  0xc0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0xc0, 0x0e,
  0x02, 0x00
};

const unsigned char kNL80211_CMD_DEAUTHENTICATE[] = {
  0x44, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x27, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x33, 0x00,
  0xc0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0xc0, 0x0e,
  0x02, 0x00, 0x00, 0x00,
};


// wlan0 (phy #0): disconnected (by AP) reason: 2: Previous authentication no
// longer valid

const uint16_t kExpectedDisconnectReason = 2;

const unsigned char kNL80211_CMD_DISCONNECT[] = {
  0x30, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x30, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x06, 0x00, 0x36, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x47, 0x00,
};


// wlan0 (phy #0): connection quality monitor event: peer c0:3f:0e:77:e8:7f
// didn't ACK 50 packets

const uint32_t kExpectedCqmNotAcked = 50;

const unsigned char kNL80211_CMD_NOTIFY_CQM[] = {
  0x3c, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x40, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x06, 0x00,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x00, 0x00,
  0x0c, 0x00, 0x5e, 0x00, 0x08, 0x00, 0x04, 0x00,
  0x32, 0x00, 0x00, 0x00,
};


// wlan0 (phy #0): disassoc 48:5d:60:77:2d:cf -> c0:3f:0e:77:e8:7f reason 3:
// Deauthenticated because sending station is  [frame: a0 00 00 00 c0 3f 0e
// 77 e8 7f 48 5d 60 77 2d cf c0 3f 0e 77 e8 7f 00 00 03 00]

const unsigned char kDisassociateFrame[] = {
  0xa0, 0x00, 0x00, 0x00, 0xc0, 0x3f, 0x0e, 0x77,
  0xe8, 0x7f, 0x48, 0x5d, 0x60, 0x77, 0x2d, 0xcf,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x00, 0x00,
  0x03, 0x00
};

const unsigned char kNL80211_CMD_DISASSOCIATE[] = {
  0x44, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x28, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x33, 0x00,
  0xa0, 0x00, 0x00, 0x00, 0xc0, 0x3f, 0x0e, 0x77,
  0xe8, 0x7f, 0x48, 0x5d, 0x60, 0x77, 0x2d, 0xcf,
  0xc0, 0x3f, 0x0e, 0x77, 0xe8, 0x7f, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00,
};

const char kGetFamilyCommandString[] = "CTRL_CMD_GETFAMILY";

}  // namespace

bool MockNetlinkSocket::SendMessage(const ByteString &out_string) {
  return true;
}

class Config80211Test : public Test {
 public:
  Config80211Test() : config80211_(Config80211::GetInstance()) {
    config80211_->message_types_[Nl80211Message::kMessageTypeString].family_id =
        kNl80211FamilyId;
    Nl80211Message::SetMessageType(
        config80211_->GetFamily(Nl80211Message::kMessageTypeString));
  }

  ~Config80211Test() {
    // Config80211 is a singleton, the sock_ field *MUST* be cleared
    // before "Config80211Test::socket_" gets invalidated, otherwise
    // later tests will refer to a corrupted memory.
    config80211_->sock_ = NULL;
  }

  void SetupConfig80211Object() {
    EXPECT_NE(reinterpret_cast<Config80211 *>(NULL), config80211_);
    config80211_->sock_ = &socket_;
    EXPECT_TRUE(config80211_->Init());
  }

 protected:
  class MockHandler80211 {
   public:
    MockHandler80211() :
      on_netlink_message_(base::Bind(&MockHandler80211::OnNetlinkMessage,
                                     base::Unretained(this))) {}
    MOCK_METHOD1(OnNetlinkMessage, void(const NetlinkMessage &msg));
    const Config80211::NetlinkMessageHandler &on_netlink_message() const {
      return on_netlink_message_;
    }
   private:
    Config80211::NetlinkMessageHandler on_netlink_message_;
    DISALLOW_COPY_AND_ASSIGN(MockHandler80211);
  };

  Config80211 *config80211_;
  MockNetlinkSocket socket_;
};

// TODO(wdg): Add a test for multi-part messages.  crbug.com/224652
// TODO(wdg): Add a test for GetFaimily.  crbug.com/224649
// TODO(wdg): Add a test for OnNewFamilyMessage.  crbug.com/222486
// TODO(wdg): Add a test for SubscribeToEvents (verify that it handles bad input
// appropriately, and that it calls NetlinkSocket::SubscribeToEvents if input
// is good.)

class TestHandlerObject {
 public:
  TestHandlerObject() :
    message_handler_(Bind(&TestHandlerObject::MessageHandler,
                          Unretained(this))) { }
  void MessageHandler(const NetlinkMessage &msg) {
  }
  const Config80211::NetlinkMessageHandler &message_handler() const {
    return message_handler_;
  }

 private:
  Config80211::NetlinkMessageHandler message_handler_;
};

// Checks a config80211 parameter to make sure it contains |handler_arg|
// in its list of broadcast handlers.
MATCHER_P(ContainsHandler, handler_arg, "") {
  if (arg == reinterpret_cast<void *>(NULL)) {
    LOG(WARNING) << "NULL parameter";
    return false;
  }
  const Config80211 *config80211 = static_cast<Config80211 *>(arg);
  const Config80211::NetlinkMessageHandler message_handler =
      static_cast<const Config80211::NetlinkMessageHandler>(handler_arg);

  return config80211->FindBroadcastHandler(message_handler);
}

TEST_F(Config80211Test, BroadcastHandlerTest) {
  SetupConfig80211Object();

  nlmsghdr *message = const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DISCONNECT));

  MockHandler80211 handler1;
  MockHandler80211 handler2;

  // Simple, 1 handler, case.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(1);
  config80211_->AddBroadcastHandler(handler1.on_netlink_message());
  config80211_->OnNlMessageReceived(message);

  // Add a second handler.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(1);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(1);
  config80211_->AddBroadcastHandler(handler2.on_netlink_message());
  config80211_->OnNlMessageReceived(message);

  // Verify that a handler can't be added twice.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(1);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(1);
  config80211_->AddBroadcastHandler(handler1.on_netlink_message());
  config80211_->OnNlMessageReceived(message);

  // Check that we can remove a handler.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(0);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(1);
  EXPECT_TRUE(config80211_->RemoveBroadcastHandler(
      handler1.on_netlink_message()));
  config80211_->OnNlMessageReceived(message);

  // Check that re-adding the handler goes smoothly.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(1);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(1);
  config80211_->AddBroadcastHandler(handler1.on_netlink_message());
  config80211_->OnNlMessageReceived(message);

  // Check that ClearBroadcastHandlers works.
  config80211_->ClearBroadcastHandlers();
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(0);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(0);
  config80211_->OnNlMessageReceived(message);
}

TEST_F(Config80211Test, MessageHandlerTest) {
  // Setup.
  SetupConfig80211Object();

  MockHandler80211 handler_broadcast;
  EXPECT_TRUE(config80211_->AddBroadcastHandler(
      handler_broadcast.on_netlink_message()));

  Nl80211Message sent_message_1(CTRL_CMD_GETFAMILY, kGetFamilyCommandString);
  MockHandler80211 handler_sent_1;

  Nl80211Message sent_message_2(CTRL_CMD_GETFAMILY, kGetFamilyCommandString);
  MockHandler80211 handler_sent_2;

  // Set up the received message as a response to sent_message_1.
  scoped_array<unsigned char> message_memory(
      new unsigned char[sizeof(kNL80211_CMD_DISCONNECT)]);
  memcpy(message_memory.get(), kNL80211_CMD_DISCONNECT,
         sizeof(kNL80211_CMD_DISCONNECT));
  nlmsghdr *received_message =
        reinterpret_cast<nlmsghdr *>(message_memory.get());

  // Now, we can start the actual test...

  // Verify that generic handler gets called for a message when no
  // message-specific handler has been installed.
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  config80211_->OnNlMessageReceived(received_message);

  // Send the message and give our handler.  Verify that we get called back.
  EXPECT_TRUE(config80211_->SendMessage(&sent_message_1,
                                        handler_sent_1.on_netlink_message()));
  // Make it appear that this message is in response to our sent message.
  received_message->nlmsg_seq = socket_.GetLastSequenceNumber();
  EXPECT_CALL(handler_sent_1, OnNetlinkMessage(_)).Times(1);
  config80211_->OnNlMessageReceived(received_message);

  // Verify that broadcast handler is called for the message after the
  // message-specific handler is called once.
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  config80211_->OnNlMessageReceived(received_message);

  // Install and then uninstall message-specific handler; verify broadcast
  // handler is called on message receipt.
  EXPECT_TRUE(config80211_->SendMessage(&sent_message_1,
                                        handler_sent_1.on_netlink_message()));
  received_message->nlmsg_seq = socket_.GetLastSequenceNumber();
  EXPECT_TRUE(config80211_->RemoveMessageHandler(sent_message_1));
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  config80211_->OnNlMessageReceived(received_message);

  // Install handler for different message; verify that broadcast handler is
  // called for _this_ message.
  EXPECT_TRUE(config80211_->SendMessage(&sent_message_2,
                                        handler_sent_2.on_netlink_message()));
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  config80211_->OnNlMessageReceived(received_message);

  // Change the ID for the message to that of the second handler; verify that
  // the appropriate handler is called for _that_ message.
  received_message->nlmsg_seq = socket_.GetLastSequenceNumber();
  EXPECT_CALL(handler_sent_2, OnNetlinkMessage(_)).Times(1);
  config80211_->OnNlMessageReceived(received_message);
}

TEST_F(Config80211Test, Parse_NL80211_CMD_TRIGGER_SCAN) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_TRIGGER_SCAN)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);

  EXPECT_EQ(NL80211_CMD_TRIGGER_SCAN, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  // Make sure the scan frequencies in the attribute are the ones we expect.
  {
    vector<uint32_t>list;
    EXPECT_TRUE(message->GetScanFrequenciesAttribute(
        NL80211_ATTR_SCAN_FREQUENCIES, &list));
    EXPECT_EQ(list.size(), arraysize(kScanFrequencyTrigger));
    int i = 0;
    vector<uint32_t>::const_iterator j = list.begin();
    while (j != list.end()) {
      EXPECT_EQ(kScanFrequencyTrigger[i], *j);
      ++i;
      ++j;
    }
  }

  {
    vector<string> ssids;
    EXPECT_TRUE(message->GetScanSsidsAttribute(NL80211_ATTR_SCAN_SSIDS,
                                               &ssids));
    EXPECT_EQ(1, ssids.size());
    EXPECT_EQ(0, ssids[0].compare(""));  // Expect a single, empty SSID.
  }

  EXPECT_TRUE(message->const_attributes()->IsFlagAttributeTrue(
      NL80211_ATTR_SUPPORT_MESH_AUTH));
}

TEST_F(Config80211Test, Parse_NL80211_CMD_NEW_SCAN_RESULTS) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_NEW_SCAN_RESULTS)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);

  EXPECT_EQ(NL80211_CMD_NEW_SCAN_RESULTS, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  // Make sure the scan frequencies in the attribute are the ones we expect.
  {
    vector<uint32_t>list;
    EXPECT_TRUE(message->GetScanFrequenciesAttribute(
        NL80211_ATTR_SCAN_FREQUENCIES, &list));
    EXPECT_EQ(arraysize(kScanFrequencyResults), list.size());
    int i = 0;
    vector<uint32_t>::const_iterator j = list.begin();
    while (j != list.end()) {
      EXPECT_EQ(kScanFrequencyResults[i], *j);
      ++i;
      ++j;
    }
  }

  {
    vector<string> ssids;
    EXPECT_TRUE(message->GetScanSsidsAttribute(NL80211_ATTR_SCAN_SSIDS,
                                               &ssids));
    EXPECT_EQ(1, ssids.size());
    EXPECT_EQ(0, ssids[0].compare(""));  // Expect a single, empty SSID.
  }

  EXPECT_TRUE(message->const_attributes()->IsFlagAttributeTrue(
      NL80211_ATTR_SUPPORT_MESH_AUTH));
}

TEST_F(Config80211Test, Parse_NL80211_CMD_NEW_STATION) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_NEW_STATION)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);
  EXPECT_EQ(NL80211_CMD_NEW_STATION, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  {
    string value;
    EXPECT_TRUE(message->GetMacAttributeString(NL80211_ATTR_MAC, &value));
    EXPECT_EQ(0, strncmp(value.c_str(), kExpectedMacAddress, value.length()));
  }

  {
    AttributeListConstRefPtr nested;
    EXPECT_TRUE(message->const_attributes()->ConstGetNestedAttributeList(
        NL80211_ATTR_STA_INFO, &nested));
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_GENERATION, &value));
    EXPECT_EQ(kNewStationExpectedGeneration, value);
  }
}

TEST_F(Config80211Test, Parse_NL80211_CMD_AUTHENTICATE) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_AUTHENTICATE)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);
  EXPECT_EQ(NL80211_CMD_AUTHENTICATE, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  {
    ByteString rawdata;
    EXPECT_TRUE(message->const_attributes()->GetRawAttributeValue(
        NL80211_ATTR_FRAME, &rawdata));
    EXPECT_FALSE(rawdata.IsEmpty());
    Nl80211Frame frame(rawdata);
    Nl80211Frame expected_frame(ByteString(kAuthenticateFrame,
                                           sizeof(kAuthenticateFrame)));
    EXPECT_TRUE(frame.IsEqual(expected_frame));
  }
}

TEST_F(Config80211Test, Parse_NL80211_CMD_ASSOCIATE) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_ASSOCIATE)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);
  EXPECT_EQ(NL80211_CMD_ASSOCIATE, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  {
    ByteString rawdata;
    EXPECT_TRUE(message->const_attributes()->GetRawAttributeValue(
        NL80211_ATTR_FRAME, &rawdata));
    EXPECT_FALSE(rawdata.IsEmpty());
    Nl80211Frame frame(rawdata);
    Nl80211Frame expected_frame(ByteString(kAssociateFrame,
                                           sizeof(kAssociateFrame)));
    EXPECT_TRUE(frame.IsEqual(expected_frame));
  }
}

TEST_F(Config80211Test, Parse_NL80211_CMD_CONNECT) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_CONNECT)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);
  EXPECT_EQ(NL80211_CMD_CONNECT, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  {
    string value;
    EXPECT_TRUE(message->GetMacAttributeString(NL80211_ATTR_MAC, &value));
    EXPECT_EQ(0, strncmp(value.c_str(), kExpectedMacAddress, value.length()));
  }

  {
    uint16_t value;
    EXPECT_TRUE(message->const_attributes()->GetU16AttributeValue(
        NL80211_ATTR_STATUS_CODE, &value));
    EXPECT_EQ(kExpectedConnectStatus, value);
  }

  // TODO(wdg): Need to check the value of this attribute.
  {
    ByteString rawdata;
    EXPECT_TRUE(message->const_attributes()->GetRawAttributeValue(
        NL80211_ATTR_RESP_IE, &rawdata));
  }
}

TEST_F(Config80211Test, Build_NL80211_CMD_CONNECT) {
  SetupConfig80211Object();

  // Build the message that is found in kNL80211_CMD_CONNECT.
  ConnectMessage message;
  EXPECT_TRUE(message.attributes()->CreateAttribute(NL80211_ATTR_WIPHY,
      Bind(&NetlinkAttribute::NewNl80211AttributeFromId)));
  EXPECT_TRUE(message.attributes()->SetU32AttributeValue(NL80211_ATTR_WIPHY,
                                                        kWiPhy));

  EXPECT_TRUE(message.attributes()->CreateAttribute(NL80211_ATTR_IFINDEX,
      Bind(&NetlinkAttribute::NewNl80211AttributeFromId)));
  EXPECT_TRUE(message.attributes()->SetU32AttributeValue(
      NL80211_ATTR_IFINDEX, kExpectedIfIndex));

  EXPECT_TRUE(message.attributes()->CreateAttribute(NL80211_ATTR_MAC,
      Bind(&NetlinkAttribute::NewNl80211AttributeFromId)));
  EXPECT_TRUE(message.attributes()->SetRawAttributeValue(NL80211_ATTR_MAC,
      ByteString(kMacAddressBytes, arraysize(kMacAddressBytes))));

  EXPECT_TRUE(message.attributes()->CreateAttribute(NL80211_ATTR_STATUS_CODE,
      Bind(&NetlinkAttribute::NewNl80211AttributeFromId)));
  EXPECT_TRUE(message.attributes()->SetU16AttributeValue(
      NL80211_ATTR_STATUS_CODE, kExpectedConnectStatus));

  EXPECT_TRUE(message.attributes()->CreateAttribute(NL80211_ATTR_RESP_IE,
      Bind(&NetlinkAttribute::NewNl80211AttributeFromId)));
  EXPECT_TRUE(message.attributes()->SetRawAttributeValue(NL80211_ATTR_RESP_IE,
      ByteString(kAssignedRespIeBytes, arraysize(kAssignedRespIeBytes))));

  // Encode the message to a ByteString and remove all the run-specific
  // values.

  ByteString message_bytes = message.Encode(config80211_->GetSequenceNumber());
  nlmsghdr *header = reinterpret_cast<nlmsghdr *>(message_bytes.GetData());
  header->nlmsg_flags = 0;  // Overwrite with known values.
  header->nlmsg_seq = 0;
  header->nlmsg_pid = 0;

  // Verify that the messages are equal.
  EXPECT_TRUE(message_bytes.Equals(
      ByteString(kNL80211_CMD_CONNECT, arraysize(kNL80211_CMD_CONNECT))));
}


TEST_F(Config80211Test, Parse_NL80211_CMD_DEAUTHENTICATE) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DEAUTHENTICATE)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);
  EXPECT_EQ(NL80211_CMD_DEAUTHENTICATE, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  {
    ByteString rawdata;
    EXPECT_TRUE(message->const_attributes()->GetRawAttributeValue(
        NL80211_ATTR_FRAME, &rawdata));
    EXPECT_FALSE(rawdata.IsEmpty());
    Nl80211Frame frame(rawdata);
    Nl80211Frame expected_frame(ByteString(kDeauthenticateFrame,
                                           sizeof(kDeauthenticateFrame)));
    EXPECT_TRUE(frame.IsEqual(expected_frame));
  }
}

TEST_F(Config80211Test, Parse_NL80211_CMD_DISCONNECT) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DISCONNECT)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);
  EXPECT_EQ(NL80211_CMD_DISCONNECT, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  {
    uint16_t value;
    EXPECT_TRUE(message->const_attributes()->GetU16AttributeValue(
        NL80211_ATTR_REASON_CODE, &value));
    EXPECT_EQ(kExpectedDisconnectReason, value);
  }

  EXPECT_TRUE(message->const_attributes()->IsFlagAttributeTrue(
      NL80211_ATTR_DISCONNECTED_BY_AP));
}

TEST_F(Config80211Test, Parse_NL80211_CMD_NOTIFY_CQM) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_NOTIFY_CQM)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);
  EXPECT_EQ(NL80211_CMD_NOTIFY_CQM, message->command());

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  {
    string value;
    EXPECT_TRUE(message->GetMacAttributeString(NL80211_ATTR_MAC, &value));
    EXPECT_EQ(0, strncmp(value.c_str(), kExpectedMacAddress, value.length()));
  }

  {
    AttributeListConstRefPtr nested;
    EXPECT_TRUE(message->const_attributes()->ConstGetNestedAttributeList(
        NL80211_ATTR_CQM, &nested));
    uint32_t threshold_event;
    EXPECT_FALSE(nested->GetU32AttributeValue(
        NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT, &threshold_event));
    uint32_t pkt_loss_event;
    EXPECT_TRUE(nested->GetU32AttributeValue(
        NL80211_ATTR_CQM_PKT_LOSS_EVENT, &pkt_loss_event));
    EXPECT_EQ(kExpectedCqmNotAcked, pkt_loss_event);
  }
}

TEST_F(Config80211Test, Parse_NL80211_CMD_DISASSOCIATE) {
  NetlinkMessage *netlink_message = NetlinkMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DISASSOCIATE)));

  EXPECT_NE(reinterpret_cast<NetlinkMessage *>(NULL), netlink_message);
  EXPECT_EQ(kNl80211FamilyId, netlink_message->message_type());
  // The follwing is legal if the message_type is kNl80211FamilyId.
  Nl80211Message *message = reinterpret_cast<Nl80211Message *>(netlink_message);
  EXPECT_EQ(NL80211_CMD_DISASSOCIATE, message->command());


  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(kWiPhy, value);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->const_attributes()->GetU32AttributeValue(
        NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(kExpectedIfIndex, value);
  }

  {
    ByteString rawdata;
    EXPECT_TRUE(message->const_attributes()->GetRawAttributeValue(
        NL80211_ATTR_FRAME, &rawdata));
    EXPECT_FALSE(rawdata.IsEmpty());
    Nl80211Frame frame(rawdata);
    Nl80211Frame expected_frame(ByteString(kDisassociateFrame,
                                           sizeof(kDisassociateFrame)));
    EXPECT_TRUE(frame.IsEqual(expected_frame));
  }
}

}  // namespace shill
