// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides tests for individual messages.  It tests
// UserBoundNlMessageFactory's ability to create specific message types and it
// tests the various UserBoundNlMessage types' ability to parse those
// messages.

// This file tests the public interface to Config80211.

#include "shill/config80211.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <netlink/attr.h>
#include <netlink/netlink.h>

#include <string>
#include <vector>

#include <base/bind.h>

#include "shill/mock_nl80211_socket.h"
#include "shill/nl80211_socket.h"
#include "shill/user_bound_nlmessage.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::vector;
using testing::_;
using testing::Return;
using testing::Test;

namespace shill {

namespace {

// These data blocks have been collected by shill using Config80211 while,
// simultaneously (and manually) comparing shill output with that of the 'iw'
// code from which it was derived.  The test strings represent the raw packet
// data coming from the kernel.  The comments above each of these strings is
// the markup that "iw" outputs for ech of these packets.

// These constants are consistent throughout the packets, below.

const uint32_t kExpectedIfIndex = 4;
const uint8_t kExpectedWifi = 0;
const char kExpectedMacAddress[] = "c0:3f:0e:77:e8:7f";


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

}  // namespace {}

class Config80211Test : public Test {
 public:
  Config80211Test() : config80211_(Config80211::GetInstance()) {}
  void SetupConfig80211Object() {
    EXPECT_NE(config80211_, reinterpret_cast<Config80211 *>(NULL));
    config80211_->sock_ = &socket_;
    EXPECT_TRUE(config80211_->Init(reinterpret_cast<EventDispatcher *>(NULL)));
    config80211_->Reset();
  }

  Config80211 *config80211_;
  MockNl80211Socket socket_;
};

class TestCallbackObject {
 public:
  TestCallbackObject() : callback_(Bind(&TestCallbackObject::MessageHandler,
                                        Unretained(this))) { }
  void MessageHandler(const UserBoundNlMessage &msg) {
  }
  const Config80211::Callback &GetCallback() const { return callback_; }

 private:
  Config80211::Callback callback_;
};

MATCHER_P(IsEqualToCallback, callback, "") {
  const Config80211::Callback *arg_cb =
      reinterpret_cast<const Config80211::Callback *>(arg);
  const Config80211::Callback *callback_cb =
      reinterpret_cast<const Config80211::Callback *>(callback);
  if (arg_cb == callback_cb)
    return true;
  if (arg_cb == reinterpret_cast<const Config80211::Callback *>(NULL))
    return false;
  if (callback_cb == reinterpret_cast<const Config80211::Callback *>(NULL))
    return arg_cb->is_null();
  return arg_cb->Equals(*callback_cb);
}

TEST_F(Config80211Test, AddLinkTest) {
  SetupConfig80211Object();

  // Create a default callback.
  TestCallbackObject callback_object;

  // Install the callback and subscribe to events using it, wifi down
  // (shouldn't actually send the subscription request).
  EXPECT_CALL(socket_, AddGroupMembership(_)).Times(0);
  EXPECT_CALL(socket_, DisableSequenceChecking()).Times(0);
  EXPECT_CALL(socket_, SetNetlinkCallback(_,_)).Times(0);

  config80211_->SetDefaultCallback(callback_object.GetCallback());
  Config80211::EventType scan_event = Config80211::kEventTypeScan;
  string scan_event_string;
  EXPECT_TRUE(Config80211::GetEventTypeString(scan_event, &scan_event_string));
  EXPECT_TRUE(config80211_->SubscribeToEvents(scan_event));

  // Wifi up, should subscribe to events.
  EXPECT_CALL(socket_, AddGroupMembership(scan_event_string))
      .WillOnce(Return(true));
  EXPECT_CALL(socket_, DisableSequenceChecking())
      .WillOnce(Return(true));
  EXPECT_CALL(socket_, SetNetlinkCallback(
      _, IsEqualToCallback(&callback_object.GetCallback())))
      .WillOnce(Return(true));
  config80211_->SetWifiState(Config80211::kWifiUp);

  // Second subscribe, same event (should do nothing).
  EXPECT_CALL(socket_, AddGroupMembership(_)).Times(0);
  EXPECT_CALL(socket_, DisableSequenceChecking()).Times(0);
  EXPECT_CALL(socket_, SetNetlinkCallback(_,_)).Times(0);
  EXPECT_TRUE(config80211_->SubscribeToEvents(scan_event));

  // Bring the wifi back down.
  config80211_->SetWifiState(Config80211::kWifiDown);

  // Subscribe to a new event with the wifi down (should still do nothing).
  Config80211::EventType mlme_event = Config80211::kEventTypeMlme;
  string mlme_event_string;
  EXPECT_TRUE(Config80211::GetEventTypeString(mlme_event, &mlme_event_string));
  EXPECT_TRUE(config80211_->SubscribeToEvents(mlme_event));

  // Wifi up (again), should subscribe to the original scan event and the new
  // mlme event.
  EXPECT_CALL(socket_, AddGroupMembership(scan_event_string))
      .WillOnce(Return(true));
  EXPECT_CALL(socket_, AddGroupMembership(mlme_event_string))
      .WillOnce(Return(true));
  EXPECT_CALL(socket_, DisableSequenceChecking())
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(socket_, SetNetlinkCallback(
      _, IsEqualToCallback(&callback_object.GetCallback())))
      .Times(2)
      .WillRepeatedly(Return(true));
  config80211_->SetWifiState(Config80211::kWifiUp);
}

TEST_F(Config80211Test, NL80211_CMD_TRIGGER_SCAN) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_TRIGGER_SCAN)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_TRIGGER_SCAN);

  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  // Make sure the scan frequencies in the attribute are the ones we expect.
  {
    vector<uint32_t>list;
    EXPECT_TRUE(message->GetScanFrequenciesAttribute(
        NL80211_ATTR_SCAN_FREQUENCIES, &list));
    EXPECT_EQ(arraysize(kScanFrequencyTrigger), list.size());
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
    EXPECT_EQ(ssids.size(), 1);
    EXPECT_EQ(ssids[0].compare(""), 0);  // Expect a single, empty SSID.
  }

  // Significant only in its existence.
  EXPECT_TRUE(message->AttributeExists(NL80211_ATTR_SUPPORT_MESH_AUTH));
}

TEST_F(Config80211Test, NL80211_CMD_NEW_SCAN_RESULTS) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_NEW_SCAN_RESULTS)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_NEW_SCAN_RESULTS);

  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
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
    EXPECT_EQ(ssids.size(), 1);
    EXPECT_EQ(ssids[0].compare(""), 0);  // Expect a single, empty SSID.
  }

  // Significant only in its existence.
  EXPECT_TRUE(message->AttributeExists(NL80211_ATTR_SUPPORT_MESH_AUTH));
}

TEST_F(Config80211Test, NL80211_CMD_NEW_STATION) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_NEW_STATION)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_NEW_STATION);

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  {
    string value;
    EXPECT_TRUE(message->GetMacAttributeString(NL80211_ATTR_MAC, &value));
    EXPECT_EQ(strncmp(value.c_str(), kExpectedMacAddress, value.length()), 0);
  }

  // TODO(wdg): Make config80211 handle nested attributes so it can deal
  // with things like NL80211_ATTR_STA_INFO (without just calling
  // nla_parse_nested).
  EXPECT_TRUE(message->AttributeExists(NL80211_ATTR_STA_INFO));

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_GENERATION, &value));
    EXPECT_EQ(value, kNewStationExpectedGeneration);
  }
}

TEST_F(Config80211Test, NL80211_CMD_AUTHENTICATE) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_AUTHENTICATE)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_AUTHENTICATE);

  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  {
    void *rawdata = NULL;
    int frame_byte_count = 0;
    EXPECT_TRUE(message->GetRawAttributeData(NL80211_ATTR_FRAME, &rawdata,
                                             &frame_byte_count));
    EXPECT_NE(rawdata, reinterpret_cast<void *>(NULL));
    const uint8_t *frame_data = reinterpret_cast<const uint8_t *>(rawdata);

    Nl80211Frame frame(frame_data, frame_byte_count);
    Nl80211Frame expected_frame(kAuthenticateFrame, sizeof(kAuthenticateFrame));

    EXPECT_TRUE(frame.IsEqual(expected_frame));
  }
}

TEST_F(Config80211Test, NL80211_CMD_ASSOCIATE) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_ASSOCIATE)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_ASSOCIATE);

  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  {
    void *rawdata = NULL;
    int frame_byte_count = 0;
    EXPECT_TRUE(message->GetRawAttributeData(NL80211_ATTR_FRAME, &rawdata,
                                             &frame_byte_count));
    EXPECT_NE(rawdata, reinterpret_cast<void *>(NULL));
    const uint8_t *frame_data = reinterpret_cast<const uint8_t *>(rawdata);

    Nl80211Frame frame(frame_data, frame_byte_count);
    Nl80211Frame expected_frame(kAssociateFrame, sizeof(kAssociateFrame));

    EXPECT_TRUE(frame.IsEqual(expected_frame));
  }
}

TEST_F(Config80211Test, NL80211_CMD_CONNECT) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_CONNECT)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_CONNECT);

  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  {
    string value;
    EXPECT_TRUE(message->GetMacAttributeString(NL80211_ATTR_MAC, &value));
    EXPECT_EQ(strncmp(value.c_str(), kExpectedMacAddress, value.length()), 0);
  }

  {
    uint16_t value;
    EXPECT_TRUE(message->GetU16Attribute(NL80211_ATTR_STATUS_CODE, &value));
    EXPECT_EQ(value, kExpectedConnectStatus);
  }

  // TODO(wdg): Need to check the value of this attribute.
  EXPECT_TRUE(message->AttributeExists(NL80211_ATTR_RESP_IE));
}

TEST_F(Config80211Test, NL80211_CMD_DEAUTHENTICATE) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DEAUTHENTICATE)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_DEAUTHENTICATE);

  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  {
    void *rawdata = NULL;
    int frame_byte_count = 0;
    EXPECT_TRUE(message->GetRawAttributeData(NL80211_ATTR_FRAME, &rawdata,
                                             &frame_byte_count));
    EXPECT_NE(rawdata, reinterpret_cast<void *>(NULL));
    const uint8_t *frame_data = reinterpret_cast<const uint8_t *>(rawdata);

    Nl80211Frame frame(frame_data, frame_byte_count);
    Nl80211Frame expected_frame(kDeauthenticateFrame,
                                sizeof(kDeauthenticateFrame));

    EXPECT_TRUE(frame.IsEqual(expected_frame));
  }
}

TEST_F(Config80211Test, NL80211_CMD_DISCONNECT) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DISCONNECT)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_DISCONNECT);

  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  {
    uint16_t value;
    EXPECT_TRUE(message->GetU16Attribute(NL80211_ATTR_REASON_CODE, &value));
    EXPECT_EQ(value, kExpectedDisconnectReason);
  }

  // Significant only in its existence.
  EXPECT_TRUE(message->AttributeExists(NL80211_ATTR_DISCONNECTED_BY_AP));
}

TEST_F(Config80211Test, NL80211_CMD_NOTIFY_CQM) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_NOTIFY_CQM)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_NOTIFY_CQM);


  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  {
    string value;
    EXPECT_TRUE(message->GetMacAttributeString(NL80211_ATTR_MAC, &value));
    EXPECT_EQ(strncmp(value.c_str(), kExpectedMacAddress, value.length()), 0);
  }

  // TODO(wdg): Make config80211 handle nested attributes so it can deal
  // with things like NL80211_ATTR_CQM (without just calling nla_parse_nested).
  {
    static const nla_policy kCqmPolicy[NL80211_ATTR_CQM_MAX + 1] = {
      { NLA_U32, 0, 0 },  // Who Knows?
      { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_THOLD]
      { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_HYST]
      { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]
    };

    EXPECT_TRUE(message->AttributeExists(NL80211_ATTR_CQM));
    const nlattr *const_data = message->GetAttribute(NL80211_ATTR_CQM);
    nlattr *cqm_attr = const_cast<nlattr *>(const_data);
    EXPECT_NE(cqm_attr, reinterpret_cast<nlattr *>(NULL));

    nlattr *cqm[NL80211_ATTR_CQM_MAX + 1];
    EXPECT_EQ(nla_parse_nested(cqm, NL80211_ATTR_CQM_MAX, cqm_attr,
                     const_cast<nla_policy *>(kCqmPolicy)), 0);

    EXPECT_FALSE(cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]);
    EXPECT_TRUE(cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT]);
    EXPECT_EQ(nla_get_u32(cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT]),
              kExpectedCqmNotAcked);
  }
}

TEST_F(Config80211Test, NL80211_CMD_DISASSOCIATE) {
  UserBoundNlMessage *message = UserBoundNlMessageFactory::CreateMessage(
      const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DISASSOCIATE)));

  EXPECT_NE(message, reinterpret_cast<UserBoundNlMessage *>(NULL));
  EXPECT_EQ(message->GetMessageType(), NL80211_CMD_DISASSOCIATE);


  {
    uint8_t value;
    EXPECT_TRUE(message->GetU8Attribute(NL80211_ATTR_WIPHY, &value));
    EXPECT_EQ(value, kExpectedWifi);
  }

  {
    uint32_t value;
    EXPECT_TRUE(message->GetU32Attribute(NL80211_ATTR_IFINDEX, &value));
    EXPECT_EQ(value, kExpectedIfIndex);
  }

  {
    void *rawdata = NULL;
    int frame_byte_count = 0;
    EXPECT_TRUE(message->GetRawAttributeData(NL80211_ATTR_FRAME, &rawdata,
                                             &frame_byte_count));
    EXPECT_NE(rawdata, reinterpret_cast<void *>(NULL));
    const uint8_t *frame_data = reinterpret_cast<const uint8_t *>(rawdata);

    Nl80211Frame frame(frame_data, frame_byte_count);
    Nl80211Frame expected_frame(kDisassociateFrame, sizeof(kDisassociateFrame));

    EXPECT_TRUE(frame.IsEqual(expected_frame));
  }
}

}  // namespace shill
