// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/wake_on_wifi.h"

#include <linux/nl80211.h>
#include <netlink/netlink.h>

#include <set>
#include <string>
#include <utility>

#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/ip_address_store.h"
#include "shill/logging.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_log.h"
#include "shill/mock_metrics.h"
#include "shill/net/byte_string.h"
#include "shill/net/ip_address.h"
#include "shill/net/mock_netlink_manager.h"
#include "shill/net/netlink_message_matchers.h"
#include "shill/net/nl80211_message.h"
#include "shill/nice_mock_control.h"
#include "shill/testing.h"

using base::Bind;
using base::Closure;
using base::Unretained;
using std::set;
using std::string;
using testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;
using ::testing::Return;

namespace shill {

namespace {

const uint16_t kNl80211FamilyId = 0x13;

// Zero-byte pattern prefixes to match the offsetting bytes in the Ethernet
// frame that lie before the source IP address field.
const uint8_t kIPV4PatternPrefix[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t kIPV6PatternPrefix[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// These masks have bits set to 1 to match bytes in an IP address pattern that
// represent the source IP address of the frame. They are padded with zero
// bits in front to ignore the frame offset and at the end to byte-align the
// mask itself.
const uint8_t kIPV4MaskBytes[] = {0x00, 0x00, 0x00, 0x3c};
const uint8_t kIPV6MaskBytes[] = {0x00, 0x00, 0xc0, 0xff, 0x3f};

const char kIPV4Address0[] = "192.168.10.20";
const uint8_t kIPV4Address0Bytes[] = {0xc0, 0xa8, 0x0a, 0x14};
const char kIPV4Address1[] = "1.2.3.4";
const uint8_t kIPV4Address1Bytes[] = {0x01, 0x02, 0x03, 0x04};

const char kIPV6Address0[] = "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210";
const uint8_t kIPV6Address0Bytes[] = {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54,
                                      0x32, 0x10, 0xfe, 0xdc, 0xba, 0x98,
                                      0x76, 0x54, 0x32, 0x10};
const char kIPV6Address1[] = "1080:0:0:0:8:800:200C:417A";
const uint8_t kIPV6Address1Bytes[] = {0x10, 0x80, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x08, 0x08, 0x00,
                                      0x20, 0x0c, 0x41, 0x7a};
const char kIPV6Address2[] = "1080::8:800:200C:417A";
const uint8_t kIPV6Address2Bytes[] = {0x10, 0x80, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x08, 0x08, 0x00,
                                      0x20, 0x0c, 0x41, 0x7a};
const char kIPV6Address3[] = "FF01::101";
const uint8_t kIPV6Address3Bytes[] = {0xff, 0x01, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x01, 0x01};
const char kIPV6Address4[] = "::1";
const uint8_t kIPV6Address4Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x01};
const char kIPV6Address5[] = "::";
const uint8_t kIPV6Address5Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00};
const char kIPV6Address6[] = "0:0:0:0:0:FFFF:129.144.52.38";
const uint8_t kIPV6Address6Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
                                      0x81, 0x90, 0x34, 0x26};
const char kIPV6Address7[] = "::DEDE:190.144.52.38";
const uint8_t kIPV6Address7Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0xde, 0xde,
                                      0xbe, 0x90, 0x34, 0x26};

// These blobs represent NL80211 messages from the kernel reporting the NIC's
// wake-on-packet settings, sent in response to NL80211_CMD_GET_WOWLAN requests.
const uint8_t kResponseNoIPAddresses[] = {
    0x14, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00};
const uint8_t kResponseIPV40[] = {
    0x4C, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0x38, 0x00,
    0x75, 0x00, 0x34, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00, 0x08,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8, 0x0A, 0x14, 0x00, 0x00};
const uint8_t kResponseIPV40WakeOnDisconnect[] = {
    0x50, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0x3C, 0x00, 0x75, 0x00,
    0x04, 0x00, 0x02, 0x00, 0x34, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0xA8, 0x0A, 0x14, 0x00, 0x00};
const uint8_t kResponseIPV401[] = {
    0x7C, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0x68, 0x00, 0x75, 0x00,
    0x64, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x00, 0x00, 0x30, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8,
    0x0A, 0x14, 0x00, 0x00};
const uint8_t kResponseIPV401IPV60[] = {
    0xB8, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0xA4, 0x00, 0x75, 0x00,
    0xA0, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x00, 0x00, 0x30, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8,
    0x0A, 0x14, 0x00, 0x00, 0x3C, 0x00, 0x03, 0x00, 0x09, 0x00, 0x01, 0x00,
    0x00, 0x00, 0xC0, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xDC,
    0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54,
    0x32, 0x10, 0x00, 0x00};
const uint8_t kResponseIPV401IPV601[] = {
    0xF4, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0xE0, 0x00, 0x75, 0x00,
    0xDC, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x00, 0x00, 0x3C, 0x00, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00,
    0x00, 0x00, 0xC0, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x00, 0x20, 0x0C,
    0x41, 0x7A, 0x00, 0x00, 0x30, 0x00, 0x03, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8,
    0x0A, 0x14, 0x00, 0x00, 0x3C, 0x00, 0x04, 0x00, 0x09, 0x00, 0x01, 0x00,
    0x00, 0x00, 0xC0, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xDC,
    0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54,
    0x32, 0x10, 0x00, 0x00};

// Bytes representing a NL80211_CMD_NEW_WIPHY reporting the wifi capabilities
// of a NIC with wiphy index |kNewWiphyNlMsg_WiphyIndex|. This message reports
// that the NIC can wake on up to |kNewWiphyNlMsg_MaxPatterns| registered
// patterns, and supports both wake on disconnect and wake on pattern.
const uint8_t kNewWiphyNlMsg[] = {
    0xc4, 0x0d, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0xdf, 0x49, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x09, 0x00, 0x02, 0x00, 0x70, 0x68, 0x79, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x2e, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x3d, 0x00, 0x07, 0x00, 0x00, 0x00, 0x05, 0x00, 0x3e, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x3f, 0x00, 0xff, 0xff, 0xff, 0xff,
    0x08, 0x00, 0x40, 0x00, 0xff, 0xff, 0xff, 0xff, 0x05, 0x00, 0x59, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x2b, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x7b, 0x00, 0x14, 0x00, 0x00, 0x00, 0x06, 0x00, 0x38, 0x00,
    0x89, 0x01, 0x00, 0x00, 0x06, 0x00, 0x7c, 0x00, 0xe6, 0x01, 0x00, 0x00,
    0x05, 0x00, 0x85, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x00, 0x68, 0x00,
    0x04, 0x00, 0x82, 0x00, 0x1c, 0x00, 0x39, 0x00, 0x04, 0xac, 0x0f, 0x00,
    0x02, 0xac, 0x0f, 0x00, 0x01, 0xac, 0x0f, 0x00, 0x05, 0xac, 0x0f, 0x00,
    0x06, 0xac, 0x0f, 0x00, 0x01, 0x72, 0x14, 0x00, 0x05, 0x00, 0x56, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x66, 0x00, 0x08, 0x00, 0x71, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x24, 0x00, 0x20, 0x00, 0x04, 0x00, 0x01, 0x00, 0x04, 0x00, 0x02, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x06, 0x00,
    0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x09, 0x00, 0x04, 0x00, 0x0a, 0x00,
    0xa8, 0x05, 0x16, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x14, 0x00, 0x03, 0x00,
    0xff, 0xff, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x04, 0x00, 0xe2, 0x11, 0x00, 0x00,
    0x05, 0x00, 0x05, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x20, 0x01, 0x01, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x6c, 0x09, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x71, 0x09, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x14, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00, 0x76, 0x09, 0x00, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x14, 0x00, 0x03, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x7b, 0x09, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x14, 0x00, 0x04, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x80, 0x09, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x14, 0x00, 0x05, 0x00, 0x08, 0x00, 0x01, 0x00, 0x85, 0x09, 0x00, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x14, 0x00, 0x06, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x8a, 0x09, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x14, 0x00, 0x07, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x8f, 0x09, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x14, 0x00, 0x08, 0x00, 0x08, 0x00, 0x01, 0x00, 0x94, 0x09, 0x00, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x14, 0x00, 0x09, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x99, 0x09, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x14, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x9e, 0x09, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x20, 0x00, 0x0b, 0x00, 0x08, 0x00, 0x01, 0x00, 0xa3, 0x09, 0x00, 0x00,
    0x04, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x0c, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xa8, 0x09, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0xa0, 0x00, 0x02, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x14, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00,
    0x10, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00, 0x37, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x02, 0x00, 0x10, 0x00, 0x03, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x6e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x0c, 0x00, 0x04, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x05, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x5a, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x06, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x78, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xb4, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x08, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x09, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x68, 0x01, 0x00, 0x00, 0x0c, 0x00, 0x0a, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x0c, 0x00, 0x0b, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x1c, 0x02, 0x00, 0x00, 0xb4, 0x03, 0x01, 0x00,
    0x14, 0x00, 0x03, 0x00, 0xff, 0xff, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x2c, 0x01, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x04, 0x00,
    0xe2, 0x11, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x06, 0x00, 0x05, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x00,
    0xfa, 0xff, 0x00, 0x00, 0xfa, 0xff, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00,
    0xa0, 0x71, 0x80, 0x03, 0x0c, 0x03, 0x01, 0x00, 0x1c, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x3c, 0x14, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x1c, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x50, 0x14, 0x00, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x1c, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x64, 0x14, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x1c, 0x00, 0x03, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x78, 0x14, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x20, 0x00, 0x04, 0x00, 0x08, 0x00, 0x01, 0x00, 0x8c, 0x14, 0x00, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x05, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xa0, 0x14, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x06, 0x00, 0x08, 0x00, 0x01, 0x00,
    0xb4, 0x14, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x20, 0x00, 0x07, 0x00, 0x08, 0x00, 0x01, 0x00, 0xc8, 0x14, 0x00, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x08, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x7c, 0x15, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x09, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x90, 0x15, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x20, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x01, 0x00, 0xa4, 0x15, 0x00, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x0b, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xb8, 0x15, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x0c, 0x00, 0x08, 0x00, 0x01, 0x00,
    0xcc, 0x15, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x24, 0x00, 0x0d, 0x00, 0x08, 0x00, 0x01, 0x00, 0xe0, 0x15, 0x00, 0x00,
    0x04, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x24, 0x00, 0x0e, 0x00, 0x08, 0x00, 0x01, 0x00, 0xf4, 0x15, 0x00, 0x00,
    0x04, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x24, 0x00, 0x0f, 0x00, 0x08, 0x00, 0x01, 0x00, 0x08, 0x16, 0x00, 0x00,
    0x04, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x01, 0x00, 0x1c, 0x16, 0x00, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x11, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x30, 0x16, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x20, 0x00, 0x12, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x44, 0x16, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x20, 0x00, 0x13, 0x00, 0x08, 0x00, 0x01, 0x00, 0x58, 0x16, 0x00, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x1c, 0x00, 0x14, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x71, 0x16, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x1c, 0x00, 0x15, 0x00, 0x08, 0x00, 0x01, 0x00, 0x85, 0x16, 0x00, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x1c, 0x00, 0x16, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x99, 0x16, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x1c, 0x00, 0x17, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xad, 0x16, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x08, 0x00, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00,
    0x1c, 0x00, 0x18, 0x00, 0x08, 0x00, 0x01, 0x00, 0xc1, 0x16, 0x00, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x08, 0x00, 0x06, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x64, 0x00, 0x02, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x01, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x5a, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x02, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x78, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x03, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xb4, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x04, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x05, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x68, 0x01, 0x00, 0x00, 0x0c, 0x00, 0x06, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x1c, 0x02, 0x00, 0x00, 0xdc, 0x00, 0x32, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x04, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x08, 0x00, 0x05, 0x00,
    0x13, 0x00, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00, 0x19, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x07, 0x00, 0x25, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00,
    0x26, 0x00, 0x00, 0x00, 0x08, 0x00, 0x09, 0x00, 0x27, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x0a, 0x00, 0x28, 0x00, 0x00, 0x00, 0x08, 0x00, 0x0b, 0x00,
    0x2b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x37, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x0d, 0x00, 0x39, 0x00, 0x00, 0x00, 0x08, 0x00, 0x0e, 0x00,
    0x3b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x0f, 0x00, 0x43, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x10, 0x00, 0x31, 0x00, 0x00, 0x00, 0x08, 0x00, 0x11, 0x00,
    0x41, 0x00, 0x00, 0x00, 0x08, 0x00, 0x12, 0x00, 0x42, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x13, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x14, 0x00,
    0x54, 0x00, 0x00, 0x00, 0x08, 0x00, 0x15, 0x00, 0x57, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x16, 0x00, 0x55, 0x00, 0x00, 0x00, 0x08, 0x00, 0x17, 0x00,
    0x59, 0x00, 0x00, 0x00, 0x08, 0x00, 0x18, 0x00, 0x5c, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x19, 0x00, 0x2d, 0x00, 0x00, 0x00, 0x08, 0x00, 0x1a, 0x00,
    0x2e, 0x00, 0x00, 0x00, 0x08, 0x00, 0x1b, 0x00, 0x30, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x6f, 0x00, 0x10, 0x27, 0x00, 0x00, 0x04, 0x00, 0x6c, 0x00,
    0x30, 0x04, 0x63, 0x00, 0x04, 0x00, 0x00, 0x00, 0x84, 0x00, 0x01, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x50, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x70, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x90, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xa0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xd0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xe0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x84, 0x00, 0x02, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x50, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x70, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x90, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xa0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xd0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xe0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x84, 0x00, 0x03, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x50, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x70, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x90, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xa0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xd0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xe0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x84, 0x00, 0x04, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x50, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x70, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x90, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xa0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xd0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xe0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x04, 0x00, 0x05, 0x00,
    0x04, 0x00, 0x06, 0x00, 0x84, 0x00, 0x07, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x50, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x70, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x80, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x90, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xa0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xf0, 0x00, 0x00, 0x00, 0x84, 0x00, 0x08, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x50, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x70, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x80, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x90, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xa0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xf0, 0x00, 0x00, 0x00, 0x84, 0x00, 0x09, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x50, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x70, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x80, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x90, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xa0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xf0, 0x00, 0x00, 0x00, 0x84, 0x00, 0x0a, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x50, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x60, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x70, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x80, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x90, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xa0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xf0, 0x00, 0x00, 0x00, 0x40, 0x01, 0x64, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x24, 0x00, 0x01, 0x00, 0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x02, 0x00, 0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x03, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xb0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xc0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xb0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xc0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x04, 0x00, 0x05, 0x00,
    0x04, 0x00, 0x06, 0x00, 0x1c, 0x00, 0x07, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xb0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xc0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x14, 0x00, 0x08, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xd0, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x09, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0x40, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xa0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xb0, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x65, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00,
    0xd0, 0x00, 0x00, 0x00, 0x14, 0x00, 0x0a, 0x00, 0x06, 0x00, 0x65, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x06, 0x00, 0x65, 0x00, 0xd0, 0x00, 0x00, 0x00,
    0x34, 0x00, 0x76, 0x00, 0x04, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x04, 0x00, 0x06, 0x00, 0x04, 0x00, 0x07, 0x00,
    0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x09, 0x00, 0x14, 0x00, 0x04, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x79, 0x00, 0x04, 0x00, 0x04, 0x00,
    0x04, 0x00, 0x06, 0x00, 0x60, 0x00, 0x78, 0x00, 0x5c, 0x00, 0x01, 0x00,
    0x48, 0x00, 0x01, 0x00, 0x14, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00, 0x04, 0x00, 0x02, 0x00,
    0x1c, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x08, 0x00,
    0x04, 0x00, 0x09, 0x00, 0x14, 0x00, 0x03, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00, 0x04, 0x00, 0x0a, 0x00,
    0x08, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x8f, 0x00, 0xe3, 0x1a, 0x00, 0x00,
    0x1e, 0x00, 0x94, 0x00, 0x62, 0x08, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0xa9, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x0c, 0x00, 0xaa, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40};
const uint32_t kNewWiphyNlMsg_WiphyIndex = 2;
const uint32_t kNewWiphyNlMsg_MaxPatterns = 20;
const int kNewWiphyNlMsg_Nl80211AttrWiphyOffset = 20;
const int kNewWiphyNlMsg_PattSupportOffset = 3336;
const int kNewWiphyNlMsg_WowlanTrigDisconnectAttributeOffset = 3304;
const uint32_t kTimeToNextLeaseRenewalShort = 1;
const uint32_t kTimeToNextLeaseRenewalLong = 1000;

}  // namespace

class WakeOnWiFiTest : public ::testing::Test {
 public:
  WakeOnWiFiTest() : metrics_(nullptr) {}
  virtual ~WakeOnWiFiTest() {}

  virtual void SetUp() {
    Nl80211Message::SetMessageType(kNl80211FamilyId);
    // Assume our NIC has reported its wiphy index, and that it supports wake
    // all wake triggers.
    wake_on_wifi_->wiphy_index_received_ = true;
    wake_on_wifi_->wake_on_wifi_triggers_supported_.insert(
        WakeOnWiFi::kPattern);
    wake_on_wifi_->wake_on_wifi_triggers_supported_.insert(
        WakeOnWiFi::kDisconnect);
    wake_on_wifi_->wake_on_wifi_triggers_supported_.insert(WakeOnWiFi::kSSID);

    ON_CALL(netlink_manager_, SendNl80211Message(_, _, _, _))
        .WillByDefault(Return(true));
  }

  void EnableWakeOnWiFiFeaturesPacket() {
    wake_on_wifi_->wake_on_wifi_features_enabled_ =
        kWakeOnWiFiFeaturesEnabledPacket;
  }

  void EnableWakeOnWiFiFeaturesSSID() {
    wake_on_wifi_->wake_on_wifi_features_enabled_ =
        kWakeOnWiFiFeaturesEnabledSSID;
  }

  void EnableWakeOnWiFiFeaturesPacketSSID() {
    wake_on_wifi_->wake_on_wifi_features_enabled_ =
        kWakeOnWiFiFeaturesEnabledPacketSSID;
  }

  void SetWakeOnWiFiFeaturesNotSupported() {
    wake_on_wifi_->wake_on_wifi_features_enabled_ =
        kWakeOnWiFiFeaturesEnabledNotSupported;
  }

  void DisableWakeOnWiFiFeatures() {
    wake_on_wifi_->wake_on_wifi_features_enabled_ =
        kWakeOnWiFiFeaturesEnabledNone;
  }

  void AddWakeOnPacketConnection(const string &ip_endpoint, Error *error) {
    wake_on_wifi_->AddWakeOnPacketConnection(ip_endpoint, error);
  }

  void RemoveWakeOnPacketConnection(const string &ip_endpoint, Error *error) {
    wake_on_wifi_->RemoveWakeOnPacketConnection(ip_endpoint, error);
  }

  void RemoveAllWakeOnPacketConnections(Error *error) {
    wake_on_wifi_->RemoveAllWakeOnPacketConnections(error);
  }

  bool CreateIPAddressPatternAndMask(const IPAddress &ip_addr,
                                     ByteString *pattern, ByteString *mask) {
    return WakeOnWiFi::CreateIPAddressPatternAndMask(ip_addr, pattern, mask);
  }

  bool ConfigureWiphyIndex(Nl80211Message *msg, int32_t index) {
    return WakeOnWiFi::ConfigureWiphyIndex(msg, index);
  }

  bool ConfigureDisableWakeOnWiFiMessage(SetWakeOnPacketConnMessage *msg,
                                         uint32_t wiphy_index, Error *error) {
    return WakeOnWiFi::ConfigureDisableWakeOnWiFiMessage(msg, wiphy_index,
                                                         error);
  }

  bool WakeOnWiFiSettingsMatch(const Nl80211Message &msg,
                               const set<WakeOnWiFi::WakeOnWiFiTrigger> &trigs,
                               const IPAddressStore &addrs) {
    return WakeOnWiFi::WakeOnWiFiSettingsMatch(msg, trigs, addrs);
  }

  bool ConfigureSetWakeOnWiFiSettingsMessage(
      SetWakeOnPacketConnMessage *msg,
      const set<WakeOnWiFi::WakeOnWiFiTrigger> &trigs,
      const IPAddressStore &addrs, uint32_t wiphy_index, Error *error) {
    return WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(
        msg, trigs, addrs, wiphy_index, error);
  }

  void RequestWakeOnPacketSettings() {
    wake_on_wifi_->RequestWakeOnPacketSettings();
  }

  void VerifyWakeOnWiFiSettings(const Nl80211Message &nl80211_message) {
    wake_on_wifi_->VerifyWakeOnWiFiSettings(nl80211_message);
  }

  size_t GetWakeOnWiFiMaxPatterns() {
    return wake_on_wifi_->wake_on_wifi_max_patterns_;
  }

  void SetWakeOnWiFiMaxPatterns(size_t max_patterns) {
    wake_on_wifi_->wake_on_wifi_max_patterns_ = max_patterns;
  }

  void ApplyWakeOnWiFiSettings() { wake_on_wifi_->ApplyWakeOnWiFiSettings(); }

  void DisableWakeOnWiFi() { wake_on_wifi_->DisableWakeOnWiFi(); }

  set<WakeOnWiFi::WakeOnWiFiTrigger> *GetWakeOnWiFiTriggers() {
    return &wake_on_wifi_->wake_on_wifi_triggers_;
  }

  set<WakeOnWiFi::WakeOnWiFiTrigger> *GetWakeOnWiFiTriggersSupported() {
    return &wake_on_wifi_->wake_on_wifi_triggers_supported_;
  }

  void ClearWakeOnWiFiTriggersSupported() {
    wake_on_wifi_->wake_on_wifi_triggers_supported_.clear();
  }

  IPAddressStore *GetWakeOnPacketConnections() {
    return &wake_on_wifi_->wake_on_packet_connections_;
  }

  void RetrySetWakeOnPacketConnections() {
    wake_on_wifi_->RetrySetWakeOnPacketConnections();
  }

  void SetSuspendActionsDoneCallback() {
    wake_on_wifi_->suspend_actions_done_callback_ =
        Bind(&WakeOnWiFiTest::DoneCallback, Unretained(this));
  }

  void ResetSuspendActionsDoneCallback() {
    wake_on_wifi_->suspend_actions_done_callback_.Reset();
  }

  bool SuspendActionsCallbackIsNull() {
    return wake_on_wifi_->suspend_actions_done_callback_.is_null();
  }

  void RunSuspendActionsCallback(const Error &error) {
    wake_on_wifi_->suspend_actions_done_callback_.Run(error);
  }

  int GetNumSetWakeOnPacketRetries() {
    return wake_on_wifi_->num_set_wake_on_packet_retries_;
  }

  void SetNumSetWakeOnPacketRetries(int retries) {
    wake_on_wifi_->num_set_wake_on_packet_retries_ = retries;
  }

  void OnBeforeSuspend(bool is_connected,
                       bool has_service_configured_for_autoconnect,
                       bool have_dhcp_lease,
                       uint32_t time_to_next_lease_renewal) {
    ResultCallback done_callback(
        Bind(&WakeOnWiFiTest::DoneCallback, Unretained(this)));
    Closure renew_dhcp_lease_callback(
        Bind(&WakeOnWiFiTest::RenewDHCPLeaseCallback, Unretained(this)));
    Closure remove_supplicant_networks_callback(Bind(
        &WakeOnWiFiTest::RemoveSupplicantNetworksCallback, Unretained(this)));
    wake_on_wifi_->OnBeforeSuspend(
        is_connected, has_service_configured_for_autoconnect, done_callback,
        renew_dhcp_lease_callback, remove_supplicant_networks_callback,
        have_dhcp_lease, time_to_next_lease_renewal);
  }

  void OnDarkResume(bool is_connected,
                    bool has_service_configured_for_autoconnect) {
    ResultCallback done_callback(
        Bind(&WakeOnWiFiTest::DoneCallback, Unretained(this)));
    Closure renew_dhcp_lease_callback(
        Bind(&WakeOnWiFiTest::RenewDHCPLeaseCallback, Unretained(this)));
    Closure initiate_scan_callback(
        Bind(&WakeOnWiFiTest::InitiateScanCallback, Unretained(this)));
    Closure remove_supplicant_networks_callback(Bind(
        &WakeOnWiFiTest::RemoveSupplicantNetworksCallback, Unretained(this)));
    wake_on_wifi_->OnDarkResume(
        is_connected, has_service_configured_for_autoconnect, done_callback,
        renew_dhcp_lease_callback, initiate_scan_callback,
        remove_supplicant_networks_callback);
  }

  void OnAfterResume() { wake_on_wifi_->OnAfterResume(); }

  void BeforeSuspendActions(bool is_connected,
                            bool has_service_configured_for_autoconnect,
                            bool start_lease_renewal_timer,
                            uint32_t time_to_next_lease_renewal) {
    SetDarkResumeActionsTimeOutCallback();
    EXPECT_FALSE(DarkResumeActionsTimeOutCallbackIsCancelled());
    Closure remove_supplicant_networks_callback(Bind(
        &WakeOnWiFiTest::RemoveSupplicantNetworksCallback, Unretained(this)));
    wake_on_wifi_->BeforeSuspendActions(
        is_connected, has_service_configured_for_autoconnect,
        start_lease_renewal_timer, time_to_next_lease_renewal,
        remove_supplicant_networks_callback);
    EXPECT_TRUE(DarkResumeActionsTimeOutCallbackIsCancelled());
  }

  void OnDHCPLeaseObtained(bool start_lease_renewal_timer,
                           uint32_t time_to_next_lease_renewal) {
    wake_on_wifi_->OnDHCPLeaseObtained(start_lease_renewal_timer,
                                       time_to_next_lease_renewal);
  }

  void SetInDarkResume(bool val) { wake_on_wifi_->in_dark_resume_ = val; }

  bool GetInDarkResume() { return wake_on_wifi_->in_dark_resume_; }

  void SetWiphyIndexReceivedToFalse() {
    wake_on_wifi_->wiphy_index_received_ = false;
  }

  uint32_t GetWiphyIndex() { return wake_on_wifi_->wiphy_index_; }

  bool GetWiphyIndexReceived() { return wake_on_wifi_->wiphy_index_received_; }

  void ParseWiphyIndex(const Nl80211Message &nl80211_message) {
    wake_on_wifi_->ParseWiphyIndex(nl80211_message);
  }

  void ParseWakeOnWiFiCapabilities(const Nl80211Message &nl80211_message) {
    wake_on_wifi_->ParseWakeOnWiFiCapabilities(nl80211_message);
  }

  bool SetWakeOnWiFiFeaturesEnabled(const std::string &enabled, Error *error) {
    return wake_on_wifi_->SetWakeOnWiFiFeaturesEnabled(enabled, error);
  }

  const string &GetWakeOnWiFiFeaturesEnabled() {
    return wake_on_wifi_->wake_on_wifi_features_enabled_;
  }

  void SetDarkResumeActionsTimeOutCallback() {
    wake_on_wifi_->dark_resume_actions_timeout_callback_.Reset(Bind(
        &WakeOnWiFiTest::DarkResumeActionsTimeoutCallback, Unretained(this)));
  }

  bool DarkResumeActionsTimeOutCallbackIsCancelled() {
    return wake_on_wifi_->dark_resume_actions_timeout_callback_.IsCancelled();
  }

  void StartDHCPLeaseRenewalTimer() {
    wake_on_wifi_->dhcp_lease_renewal_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kTimeToNextLeaseRenewalLong),
        Bind(&WakeOnWiFiTest::OnTimerWakeDoNothing, Unretained(this)));
  }

  void StartWakeToScanTimer() {
    wake_on_wifi_->wake_to_scan_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kTimeToNextLeaseRenewalLong),
        Bind(&WakeOnWiFiTest::OnTimerWakeDoNothing, Unretained(this)));
  }

  void StopDHCPLeaseRenewalTimer() {
    wake_on_wifi_->dhcp_lease_renewal_timer_.Stop();
  }

  void StopWakeToScanTimer() { wake_on_wifi_->wake_to_scan_timer_.Stop(); }

  bool DHCPLeaseRenewalTimerIsRunning() {
    return wake_on_wifi_->dhcp_lease_renewal_timer_.IsRunning();
  }

  bool WakeToScanTimerIsRunning() {
    return wake_on_wifi_->wake_to_scan_timer_.IsRunning();
  }

  void SetDarkResumeActionsTimeoutMilliseconds(int64_t timeout) {
    wake_on_wifi_->DarkResumeActionsTimeoutMilliseconds = timeout;
  }

  void InitStateForDarkResume() {
    SetInDarkResume(true);
    GetWakeOnPacketConnections()->AddUnique(IPAddress("1.1.1.1"));
    EnableWakeOnWiFiFeaturesPacketSSID();
    SetDarkResumeActionsTimeoutMilliseconds(0);
  }

  void SetExpectationsDisconnectedBeforeSuspend() {
    EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
    EXPECT_CALL(*this, DoneCallback(_)).Times(0);
    EXPECT_CALL(*this, RemoveSupplicantNetworksCallback()).Times(1);
    EXPECT_CALL(netlink_manager_,
                SendNl80211Message(
                    IsNl80211Command(kNl80211FamilyId,
                                     SetWakeOnPacketConnMessage::kCommand),
                    _, _, _));
  }

  void SetExpectationsConnectedBeforeSuspend() {
    EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
    EXPECT_CALL(*this, DoneCallback(_)).Times(0);
    EXPECT_CALL(netlink_manager_,
                SendNl80211Message(
                    IsNl80211Command(kNl80211FamilyId,
                                     SetWakeOnPacketConnMessage::kCommand),
                    _, _, _));
  }

  void VerifyStateConnectedBeforeSuspend() {
    EXPECT_TRUE(DarkResumeActionsTimeOutCallbackIsCancelled());
    EXPECT_FALSE(GetInDarkResume());
    EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 2);
    EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kPattern) !=
                GetWakeOnWiFiTriggers()->end());
    EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kDisconnect) !=
                GetWakeOnWiFiTriggers()->end());
  }

  void VerifyStateDisconnectedBeforeSuspend() {
    EXPECT_TRUE(DarkResumeActionsTimeOutCallbackIsCancelled());
    EXPECT_FALSE(GetInDarkResume());
    EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 1);
    EXPECT_FALSE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kPattern) !=
                 GetWakeOnWiFiTriggers()->end());
    EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kSSID) !=
                GetWakeOnWiFiTriggers()->end());
  }

  void ReportConnectedToServiceAfterWake(bool is_connected) {
    wake_on_wifi_->ReportConnectedToServiceAfterWake(is_connected);
  }

  void OnNoAutoConnectableServicesAfterScan(
      bool has_service_configured_for_autoconnect,
      const Closure &remove_supplicant_networks_callback) {
    wake_on_wifi_->OnNoAutoConnectableServicesAfterScan(
        has_service_configured_for_autoconnect,
        remove_supplicant_networks_callback);
  }

  EventHistory *GetDarkResumesSinceLastSuspend() {
    return &wake_on_wifi_->dark_resumes_since_last_suspend_;
  }

  MOCK_METHOD1(DoneCallback, void(const Error &error));
  MOCK_METHOD0(RenewDHCPLeaseCallback, void(void));
  MOCK_METHOD0(InitiateScanCallback, void(void));
  MOCK_METHOD0(RemoveSupplicantNetworksCallback, void(void));
  MOCK_METHOD0(DarkResumeActionsTimeoutCallback, void(void));
  MOCK_METHOD0(OnTimerWakeDoNothing, void(void));

 protected:
  NiceMockControl control_interface_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockNetlinkManager netlink_manager_;
  std::unique_ptr<WakeOnWiFi> wake_on_wifi_;
};

class WakeOnWiFiTestWithDispatcher : public WakeOnWiFiTest {
 public:
  WakeOnWiFiTestWithDispatcher() : WakeOnWiFiTest() {
    wake_on_wifi_.reset(
        new WakeOnWiFi(&netlink_manager_, &dispatcher_, &metrics_));
  }
  virtual ~WakeOnWiFiTestWithDispatcher() {}

 protected:
  EventDispatcher dispatcher_;
};

class WakeOnWiFiTestWithMockDispatcher : public WakeOnWiFiTest {
 public:
  WakeOnWiFiTestWithMockDispatcher() : WakeOnWiFiTest() {
    wake_on_wifi_.reset(
        new WakeOnWiFi(&netlink_manager_, &mock_dispatcher_, &metrics_));
  }
  virtual ~WakeOnWiFiTestWithMockDispatcher() {}

 protected:
  MockEventDispatcher mock_dispatcher_;
};

ByteString CreatePattern(const unsigned char *prefix, size_t prefix_len,
                         const unsigned char *addr, size_t addr_len) {
  ByteString result(prefix, prefix_len);
  result.Append(ByteString(addr, addr_len));
  return result;
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, CreateIPAddressPatternAndMask) {
  ByteString pattern;
  ByteString mask;
  ByteString expected_pattern;

  CreateIPAddressPatternAndMask(IPAddress(kIPV4Address0), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address0Bytes, sizeof(kIPV4Address0Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV4Address1), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address1Bytes, sizeof(kIPV4Address1Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV6Address0), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address0Bytes, sizeof(kIPV6Address0Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV6Address1), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address1Bytes, sizeof(kIPV6Address1Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV6Address2), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address2Bytes, sizeof(kIPV6Address2Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV6Address3), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address3Bytes, sizeof(kIPV6Address3Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV6Address4), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address4Bytes, sizeof(kIPV6Address4Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV6Address5), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address5Bytes, sizeof(kIPV6Address5Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV6Address6), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address6Bytes, sizeof(kIPV6Address6Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  CreateIPAddressPatternAndMask(IPAddress(kIPV6Address7), &pattern, &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address7Bytes, sizeof(kIPV6Address7Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, ConfigureWiphyIndex) {
  SetWakeOnPacketConnMessage msg;
  uint32_t value;
  EXPECT_FALSE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));

  ConfigureWiphyIndex(&msg, 137);
  EXPECT_TRUE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));
  EXPECT_EQ(value, 137);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, ConfigureDisableWakeOnWiFiMessage) {
  SetWakeOnPacketConnMessage msg;
  Error e;
  uint32_t value;
  EXPECT_FALSE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));

  ConfigureDisableWakeOnWiFiMessage(&msg, 57, &e);
  EXPECT_EQ(e.type(), Error::Type::kSuccess);
  EXPECT_TRUE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));
  EXPECT_EQ(value, 57);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, WakeOnWiFiSettingsMatch) {
  IPAddressStore all_addresses;
  set<WakeOnWiFi::WakeOnWiFiTrigger> trigs;
  GetWakeOnPacketConnMessage msg0;
  msg0.InitFromNlmsg(
      reinterpret_cast<const nlmsghdr *>(kResponseNoIPAddresses));
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  trigs.insert(WakeOnWiFi::kPattern);
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address0, sizeof(kIPV4Address0))));
  GetWakeOnPacketConnMessage msg1;
  msg1.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseIPV40));
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  // Test matching of wake-on-disconnect flag.
  trigs.insert(WakeOnWiFi::kDisconnect);
  GetWakeOnPacketConnMessage msg2;
  msg2.InitFromNlmsg(
      reinterpret_cast<const nlmsghdr *>(kResponseIPV40WakeOnDisconnect));
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  trigs.erase(WakeOnWiFi::kDisconnect);
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address1, sizeof(kIPV4Address1))));
  GetWakeOnPacketConnMessage msg3;
  msg3.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseIPV401));
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg3, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address0, sizeof(kIPV6Address0))));
  GetWakeOnPacketConnMessage msg4;
  msg4.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseIPV401IPV60));
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg4, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg3, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address1, sizeof(kIPV6Address1))));
  GetWakeOnPacketConnMessage msg5;
  msg5.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseIPV401IPV601));
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg5, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg4, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg3, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ConfigureSetWakeOnWiFiSettingsMessage) {
  IPAddressStore all_addresses;
  set<WakeOnWiFi::WakeOnWiFiTrigger> trigs;
  int index = 1;  // wiphy device number
  SetWakeOnPacketConnMessage msg0;
  Error e;
  trigs.insert(WakeOnWiFi::kPattern);
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address0, sizeof(kIPV4Address0))));
  ByteString expected_mask = ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes));
  ByteString expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address0Bytes, sizeof(kIPV4Address0Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg0, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg1;
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address1, sizeof(kIPV4Address1))));
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address1Bytes, sizeof(kIPV4Address1Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg1, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg2;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address0, sizeof(kIPV6Address0))));
  expected_mask = ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address0Bytes, sizeof(kIPV6Address0Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg2, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg3;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address1, sizeof(kIPV6Address1))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address1Bytes, sizeof(kIPV6Address1Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg3, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg3, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg4;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address2, sizeof(kIPV6Address2))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address2Bytes, sizeof(kIPV6Address2Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg4, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg4, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg5;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address3, sizeof(kIPV6Address3))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address3Bytes, sizeof(kIPV6Address3Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg5, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg5, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg6;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address4, sizeof(kIPV6Address4))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address4Bytes, sizeof(kIPV6Address4Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg6, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg6, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg7;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address5, sizeof(kIPV6Address5))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address5Bytes, sizeof(kIPV6Address5Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg7, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg7, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg8;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address6, sizeof(kIPV6Address6))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address6Bytes, sizeof(kIPV6Address6Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg8, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg8, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg9;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address7, sizeof(kIPV6Address7))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address7Bytes, sizeof(kIPV6Address7Bytes));
  ConfigureSetWakeOnWiFiSettingsMessage(&msg9, trigs, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWiFiSettingsMatch(msg9, trigs, all_addresses));
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, RequestWakeOnPacketSettings) {
  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          GetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  RequestWakeOnPacketSettings();
}

MATCHER_P(ErrorType, type, "") { return arg.type() == type; }

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       VerifyWakeOnWiFiSettings_NoWakeOnPacketRules) {
  ScopedMockLog log;
  // Create an Nl80211 response to a NL80211_CMD_GET_WOWLAN request
  // indicating that there are no wake-on-packet rules programmed into the NIC.
  GetWakeOnPacketConnMessage msg;
  msg.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseNoIPAddresses));
  // Successful verification and consequent invocation of callback.
  SetSuspendActionsDoneCallback();
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(2);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Empty());
  EXPECT_FALSE(SuspendActionsCallbackIsNull());
  EXPECT_CALL(*this, DoneCallback(ErrorType(Error::kSuccess))).Times(1);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(
      log,
      Log(_, _, HasSubstr("Wake-on-packet settings successfully verified")));
  EXPECT_CALL(metrics_, NotifyVerifyWakeOnWiFiSettingsResult(
                            Metrics::kVerifyWakeOnWiFiSettingsResultSuccess));
  VerifyWakeOnWiFiSettings(msg);
  // Suspend action callback cleared after being invoked.
  EXPECT_TRUE(SuspendActionsCallbackIsNull());
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  ScopeLogger::GetInstance()->set_verbose_level(0);

  // Unsuccessful verification if locally stored settings do not match.
  GetWakeOnPacketConnections()->AddUnique(IPAddress("1.1.1.1"));
  GetWakeOnWiFiTriggers()->insert(WakeOnWiFi::kPattern);
  EXPECT_CALL(log,
              Log(logging::LOG_ERROR, _,
                  HasSubstr(
                      " failed: discrepancy between wake-on-packet settings on "
                      "NIC and those in local data structure detected")));
  EXPECT_CALL(metrics_, NotifyVerifyWakeOnWiFiSettingsResult(
                            Metrics::kVerifyWakeOnWiFiSettingsResultFailure));
  VerifyWakeOnWiFiSettings(msg);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       VerifyWakeOnWiFiSettings_WakeOnPatternAndDisconnectRules) {
  ScopedMockLog log;
  // Create a non-trivial Nl80211 response to a NL80211_CMD_GET_WOWLAN request
  // indicating that that the NIC wakes on packets from 192.168.10.20 and on
  // disconnects.
  GetWakeOnPacketConnMessage msg;
  msg.InitFromNlmsg(
      reinterpret_cast<const nlmsghdr *>(kResponseIPV40WakeOnDisconnect));
  // Successful verification and consequent invocation of callback.
  SetSuspendActionsDoneCallback();
  EXPECT_FALSE(SuspendActionsCallbackIsNull());
  GetWakeOnPacketConnections()->AddUnique(IPAddress("192.168.10.20"));
  GetWakeOnWiFiTriggers()->insert(WakeOnWiFi::kPattern);
  GetWakeOnWiFiTriggers()->insert(WakeOnWiFi::kDisconnect);
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(2);
  EXPECT_CALL(*this, DoneCallback(ErrorType(Error::kSuccess))).Times(1);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(
      log,
      Log(_, _, HasSubstr("Wake-on-packet settings successfully verified")));
  EXPECT_CALL(metrics_, NotifyVerifyWakeOnWiFiSettingsResult(
                            Metrics::kVerifyWakeOnWiFiSettingsResultSuccess));
  VerifyWakeOnWiFiSettings(msg);
  // Suspend action callback cleared after being invoked.
  EXPECT_TRUE(SuspendActionsCallbackIsNull());
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  ScopeLogger::GetInstance()->set_verbose_level(0);

  // Unsuccessful verification if locally stored settings do not match.
  GetWakeOnWiFiTriggers()->erase(WakeOnWiFi::kDisconnect);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log,
              Log(logging::LOG_ERROR, _,
                  HasSubstr(
                      " failed: discrepancy between wake-on-packet settings on "
                      "NIC and those in local data structure detected")));
  EXPECT_CALL(metrics_, NotifyVerifyWakeOnWiFiSettingsResult(
                            Metrics::kVerifyWakeOnWiFiSettingsResultFailure));
  VerifyWakeOnWiFiSettings(msg);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       VerifyWakeOnWiFiSettingsSuccess_NoDoneCallback) {
  ScopedMockLog log;
  // Create an Nl80211 response to a NL80211_CMD_GET_WOWLAN request
  // indicating that there are no wake-on-packet rules programmed into the NIC.
  GetWakeOnPacketConnMessage msg;
  msg.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseNoIPAddresses));
  // Successful verification, but since there is no suspend action callback
  // set, no callback is invoked.
  EXPECT_TRUE(SuspendActionsCallbackIsNull());
  EXPECT_TRUE(GetWakeOnPacketConnections()->Empty());
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(2);
  EXPECT_CALL(*this, DoneCallback(_)).Times(0);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(
      log,
      Log(_, _, HasSubstr("Wake-on-packet settings successfully verified")));
  EXPECT_CALL(metrics_, NotifyVerifyWakeOnWiFiSettingsResult(
                            Metrics::kVerifyWakeOnWiFiSettingsResultSuccess));
  VerifyWakeOnWiFiSettings(msg);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  ScopeLogger::GetInstance()->set_verbose_level(0);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       RetrySetWakeOnPacketConnections_LessThanMaxRetries) {
  ScopedMockLog log;
  // Max retries not reached yet, so send Nl80211 message to program NIC again.
  GetWakeOnWiFiTriggers()->insert(WakeOnWiFi::kDisconnect);
  SetNumSetWakeOnPacketRetries(WakeOnWiFi::kMaxSetWakeOnPacketRetries - 1);
  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  RetrySetWakeOnPacketConnections();
  EXPECT_EQ(GetNumSetWakeOnPacketRetries(),
            WakeOnWiFi::kMaxSetWakeOnPacketRetries);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       RetrySetWakeOnPacketConnections_MaxAttemptsWithCallbackSet) {
  ScopedMockLog log;
  // Max retry attempts reached. Suspend actions done callback is set, so it
  // is invoked.
  SetNumSetWakeOnPacketRetries(WakeOnWiFi::kMaxSetWakeOnPacketRetries);
  SetSuspendActionsDoneCallback();
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(3);
  EXPECT_FALSE(SuspendActionsCallbackIsNull());
  EXPECT_CALL(*this, DoneCallback(ErrorType(Error::kOperationFailed))).Times(1);
  EXPECT_CALL(netlink_manager_, SendNl80211Message(_, _, _, _)).Times(0);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("max retry attempts reached")));
  RetrySetWakeOnPacketConnections();
  EXPECT_TRUE(SuspendActionsCallbackIsNull());
  EXPECT_EQ(GetNumSetWakeOnPacketRetries(), 0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  ScopeLogger::GetInstance()->set_verbose_level(0);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       RetrySetWakeOnPacketConnections_MaxAttemptsCallbackUnset) {
  ScopedMockLog log;
  // If there is no suspend action callback set, no suspend callback should be
  // invoked.
  SetNumSetWakeOnPacketRetries(WakeOnWiFi::kMaxSetWakeOnPacketRetries);
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(3);
  EXPECT_TRUE(SuspendActionsCallbackIsNull());
  EXPECT_CALL(*this, DoneCallback(_)).Times(0);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("max retry attempts reached")));
  RetrySetWakeOnPacketConnections();
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  ScopeLogger::GetInstance()->set_verbose_level(0);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, ParseWiphyIndex_Success) {
  // Verify that the wiphy index in kNewWiphyNlMsg is parsed, and that the flag
  // for having the wiphy index is set by ParseWiphyIndex.
  SetWiphyIndexReceivedToFalse();
  EXPECT_FALSE(GetWiphyIndexReceived());
  EXPECT_EQ(GetWiphyIndex(), WakeOnWiFi::kDefaultWiphyIndex);
  NewWiphyMessage msg;
  msg.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kNewWiphyNlMsg));
  ParseWiphyIndex(msg);
  EXPECT_EQ(GetWiphyIndex(), kNewWiphyNlMsg_WiphyIndex);
  EXPECT_TRUE(GetWiphyIndexReceived());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, ParseWiphyIndex_Failure) {
  ScopedMockLog log;
  SetWiphyIndexReceivedToFalse();
  EXPECT_FALSE(GetWiphyIndexReceived());
  // Change the NL80211_ATTR_WIPHY U32 attribute to the NL80211_ATTR_WIPHY_FREQ
  // U32 attribute, so that this message no longer contains a wiphy_index to be
  // parsed.
  NewWiphyMessage msg;
  uint8_t message_memory[sizeof(kNewWiphyNlMsg)];
  memcpy(message_memory, kNewWiphyNlMsg, sizeof(kNewWiphyNlMsg));
  struct nlattr *nl80211_attr_wiphy = reinterpret_cast<struct nlattr *>(
      &message_memory[kNewWiphyNlMsg_Nl80211AttrWiphyOffset]);
  nl80211_attr_wiphy->nla_type = NL80211_ATTR_WIPHY_FREQ;
  msg.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(message_memory));
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       "NL80211_CMD_NEW_WIPHY had no NL80211_ATTR_WIPHY"));
  ParseWiphyIndex(msg);
  // Since we failed to find NL80211_ATTR_WIPHY in the message,
  // |wiphy_index_received| should remain false.
  EXPECT_FALSE(GetWiphyIndexReceived());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ParseWakeOnWiFiCapabilities_DisconnectAndPatternSupported) {
  ClearWakeOnWiFiTriggersSupported();
  NewWiphyMessage msg;
  msg.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kNewWiphyNlMsg));
  ParseWakeOnWiFiCapabilities(msg);
  EXPECT_TRUE(GetWakeOnWiFiTriggersSupported()->find(WakeOnWiFi::kDisconnect) !=
              GetWakeOnWiFiTriggersSupported()->end());
  EXPECT_TRUE(GetWakeOnWiFiTriggersSupported()->find(WakeOnWiFi::kPattern) !=
              GetWakeOnWiFiTriggersSupported()->end());
  EXPECT_EQ(GetWakeOnWiFiMaxPatterns(), kNewWiphyNlMsg_MaxPatterns);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ParseWakeOnWiFiCapabilities_UnsupportedPatternLen) {
  ClearWakeOnWiFiTriggersSupported();
  NewWiphyMessage msg;
  // Modify the range of support pattern lengths to [0-1] bytes, which is less
  // than what we need to use  our IPV4 (30 bytes) or IPV6 (38 bytes) patterns.
  uint8_t message_memory[sizeof(kNewWiphyNlMsg)];
  memcpy(message_memory, kNewWiphyNlMsg, sizeof(kNewWiphyNlMsg));
  struct nl80211_pattern_support *patt_support =
      reinterpret_cast<struct nl80211_pattern_support *>(
          &message_memory[kNewWiphyNlMsg_PattSupportOffset]);
  patt_support->min_pattern_len = 0;
  patt_support->max_pattern_len = 1;
  msg.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(message_memory));
  ParseWakeOnWiFiCapabilities(msg);
  EXPECT_TRUE(GetWakeOnWiFiTriggersSupported()->find(WakeOnWiFi::kDisconnect) !=
              GetWakeOnWiFiTriggersSupported()->end());
  // Ensure that ParseWakeOnWiFiCapabilities realizes that our IP address
  // patterns cannot be used given the support pattern length range reported.
  EXPECT_FALSE(GetWakeOnWiFiTriggersSupported()->find(WakeOnWiFi::kPattern) !=
               GetWakeOnWiFiTriggersSupported()->end());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ParseWakeOnWiFiCapabilities_DisconnectNotSupported) {
  ClearWakeOnWiFiTriggersSupported();
  NewWiphyMessage msg;
  // Change the NL80211_WOWLAN_TRIG_DISCONNECT flag attribute into the
  // NL80211_WOWLAN_TRIG_MAGIC_PKT flag attribute, so that this message
  // no longer reports wake on disconnect as a supported capability.
  uint8_t message_memory[sizeof(kNewWiphyNlMsg)];
  memcpy(message_memory, kNewWiphyNlMsg, sizeof(kNewWiphyNlMsg));
  struct nlattr *wowlan_trig_disconnect_attr =
      reinterpret_cast<struct nlattr *>(
          &message_memory[kNewWiphyNlMsg_WowlanTrigDisconnectAttributeOffset]);
  wowlan_trig_disconnect_attr->nla_type = NL80211_WOWLAN_TRIG_MAGIC_PKT;
  msg.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(message_memory));
  ParseWakeOnWiFiCapabilities(msg);
  EXPECT_TRUE(GetWakeOnWiFiTriggersSupported()->find(WakeOnWiFi::kPattern) !=
              GetWakeOnWiFiTriggersSupported()->end());
  // Ensure that ParseWakeOnWiFiCapabilities realizes that wake on disconnect
  // is not supported.
  EXPECT_FALSE(
      GetWakeOnWiFiTriggersSupported()->find(WakeOnWiFi::kDisconnect) !=
      GetWakeOnWiFiTriggersSupported()->end());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ApplyWakeOnWiFiSettings_WiphyIndexNotReceived) {
  ScopedMockLog log;
  // ApplyWakeOnWiFiSettings should return immediately if the wifi interface
  // index has not been received when the function is called.
  SetWiphyIndexReceivedToFalse();
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnWiFiMsg(), _, _, _)).Times(0);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       HasSubstr("Interface index not yet received")));
  ApplyWakeOnWiFiSettings();
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ApplyWakeOnWiFiSettings_WiphyIndexReceived) {
  // Disable wake on WiFi if there are no wake on WiFi triggers registered.
  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(0);
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnWiFiMsg(), _, _, _)).Times(1);
  ApplyWakeOnWiFiSettings();

  // Otherwise, program the NIC.
  IPAddress ip_addr("1.1.1.1");
  GetWakeOnPacketConnections()->AddUnique(ip_addr);
  GetWakeOnWiFiTriggers()->insert(WakeOnWiFi::kPattern);
  EXPECT_FALSE(GetWakeOnPacketConnections()->Empty());
  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnWiFiMsg(), _, _, _)).Times(0);
  ApplyWakeOnWiFiSettings();
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       BeforeSuspendActions_ReportDoneImmediately) {
  ScopedMockLog log;
  const bool is_connected = true;
  const bool start_lease_renewal_timer = true;
  const bool has_service_configured_for_autoconnect = true;
  // If no triggers are supported, no triggers will be programmed into the NIC.
  ClearWakeOnWiFiTriggersSupported();
  SetSuspendActionsDoneCallback();
  SetInDarkResume(true);
  // Do not report done immediately in dark resume, since we need to program it
  // to disable wake on WiFi.
  EXPECT_CALL(*this, DoneCallback(_)).Times(0);
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  EXPECT_FALSE(GetInDarkResume());

  SetInDarkResume(false);
  // Report done immediately on normal suspend, since wake on WiFi should
  // already have been disabled on the NIC on a previous resume.
  EXPECT_CALL(*this, DoneCallback(_)).Times(1);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(
      log,
      Log(logging::LOG_INFO, _,
          HasSubstr(
              "No need to disable wake on WiFi on NIC in regular suspend")));
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       BeforeSuspendActions_FeaturesDisabledOrTriggersUnsupported) {
  const bool is_connected = true;
  const bool start_lease_renewal_timer = true;
  const bool has_service_configured_for_autoconnect = true;
  SetInDarkResume(false);
  SetSuspendActionsDoneCallback();
  // No features enabled, so no triggers programmed.
  DisableWakeOnWiFiFeatures();
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
  EXPECT_CALL(*this, DoneCallback(_));
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());

  // No triggers supported, so no triggers programmed.
  SetSuspendActionsDoneCallback();
  EnableWakeOnWiFiFeaturesPacketSSID();
  GetWakeOnWiFiTriggersSupported()->clear();
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
  EXPECT_CALL(*this, DoneCallback(_));
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());

  // Only wake on packet feature enabled and supported.
  EnableWakeOnWiFiFeaturesPacket();
  GetWakeOnWiFiTriggersSupported()->insert(WakeOnWiFi::kPattern);
  GetWakeOnPacketConnections()->AddUnique(IPAddress("1.1.1.1"));
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 1);
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kPattern) !=
              GetWakeOnWiFiTriggers()->end());

  // Only wake on SSID feature supported.
  EnableWakeOnWiFiFeaturesSSID();
  GetWakeOnPacketConnections()->Clear();
  GetWakeOnWiFiTriggersSupported()->clear();
  GetWakeOnWiFiTriggersSupported()->insert(WakeOnWiFi::kDisconnect);
  GetWakeOnWiFiTriggersSupported()->insert(WakeOnWiFi::kSSID);
  GetWakeOnWiFiTriggers()->clear();
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 1);
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kDisconnect) !=
              GetWakeOnWiFiTriggers()->end());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       BeforeSuspendActions_ConnectedBeforeSuspend) {
  const bool is_connected = true;
  const bool start_lease_renewal_timer = true;
  const bool has_service_configured_for_autoconnect = true;
  SetSuspendActionsDoneCallback();
  EnableWakeOnWiFiFeaturesPacketSSID();
  GetWakeOnPacketConnections()->AddUnique(IPAddress("1.1.1.1"));

  SetInDarkResume(true);
  GetWakeOnWiFiTriggers()->clear();
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
  StartWakeToScanTimer();
  StopDHCPLeaseRenewalTimer();
  EXPECT_TRUE(WakeToScanTimerIsRunning());
  EXPECT_FALSE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_CALL(*this, DoneCallback(_)).Times(0);
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  EXPECT_FALSE(GetInDarkResume());
  EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 2);
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kDisconnect) !=
              GetWakeOnWiFiTriggers()->end());
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kPattern) !=
              GetWakeOnWiFiTriggers()->end());
  EXPECT_TRUE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_FALSE(WakeToScanTimerIsRunning());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       BeforeSuspendActions_DisconnectedBeforeSuspend) {
  const bool is_connected = false;
  const bool start_lease_renewal_timer = true;
  bool has_service_configured_for_autoconnect = true;
  SetSuspendActionsDoneCallback();
  EnableWakeOnWiFiFeaturesPacketSSID();
  GetWakeOnPacketConnections()->AddUnique(IPAddress("1.1.1.1"));

  SetInDarkResume(true);
  GetWakeOnWiFiTriggers()->clear();
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
  StopWakeToScanTimer();
  StartDHCPLeaseRenewalTimer();
  EXPECT_FALSE(WakeToScanTimerIsRunning());
  EXPECT_TRUE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_CALL(*this, DoneCallback(_)).Times(0);
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  EXPECT_FALSE(GetInDarkResume());
  EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 1);
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kSSID) !=
              GetWakeOnWiFiTriggers()->end());
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kPattern) ==
              GetWakeOnWiFiTriggers()->end());
  EXPECT_FALSE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_TRUE(WakeToScanTimerIsRunning());

  // Do not start wake to scan timer if there is no service configured for
  // auto-connect.
  has_service_configured_for_autoconnect = false;
  StopWakeToScanTimer();
  EXPECT_FALSE(WakeToScanTimerIsRunning());
  BeforeSuspendActions(is_connected, has_service_configured_for_autoconnect,
                       start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  EXPECT_FALSE(WakeToScanTimerIsRunning());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, DisableWakeOnWiFi_ClearsTriggers) {
  GetWakeOnWiFiTriggers()->insert(WakeOnWiFi::kPattern);
  EXPECT_FALSE(GetWakeOnWiFiTriggers()->empty());
  DisableWakeOnWiFi();
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->empty());
}

#if !defined(DISABLE_WAKE_ON_WIFI)

TEST_F(WakeOnWiFiTestWithMockDispatcher, AddRemoveWakeOnPacketConnection) {
  const string bad_ip_string("1.1");
  const string ip_string1("192.168.0.19");
  const string ip_string2("192.168.0.55");
  const string ip_string3("192.168.0.74");
  IPAddress ip_addr1(ip_string1);
  IPAddress ip_addr2(ip_string2);
  IPAddress ip_addr3(ip_string3);
  Error e;

  // Add and remove operations will fail if we provide an invalid IP address
  // string.
  EnableWakeOnWiFiFeaturesPacket();
  AddWakeOnPacketConnection(bad_ip_string, &e);
  EXPECT_EQ(e.type(), Error::kInvalidArguments);
  EXPECT_STREQ(e.message().c_str(),
               ("Invalid ip_address " + bad_ip_string).c_str());
  RemoveWakeOnPacketConnection(bad_ip_string, &e);
  EXPECT_EQ(e.type(), Error::kInvalidArguments);
  EXPECT_STREQ(e.message().c_str(),
               ("Invalid ip_address " + bad_ip_string).c_str());

  // Add and remove operations will fail if WiFi device does not support
  // pattern matching functionality, even if the feature is enabled.
  EnableWakeOnWiFiFeaturesPacket();
  ClearWakeOnWiFiTriggersSupported();
  AddWakeOnPacketConnection(ip_string1, &e);
  EXPECT_EQ(e.type(), Error::kNotSupported);
  EXPECT_STREQ(e.message().c_str(),
               "Wake on IP address patterns not supported by this WiFi device");
  RemoveAllWakeOnPacketConnections(&e);
  EXPECT_EQ(e.type(), Error::kNotSupported);
  EXPECT_STREQ(e.message().c_str(),
               "Wake on IP address patterns not supported by this WiFi device");
  RemoveWakeOnPacketConnection(ip_string2, &e);
  EXPECT_EQ(e.type(), Error::kNotSupported);
  EXPECT_STREQ(e.message().c_str(),
               "Wake on IP address patterns not supported by this WiFi device");

  // Add operation will fail if pattern matching is supported but the max number
  // of IP address patterns have already been registered.
  EnableWakeOnWiFiFeaturesPacketSSID();
  GetWakeOnWiFiTriggersSupported()->insert(WakeOnWiFi::kPattern);
  SetWakeOnWiFiMaxPatterns(0);
  AddWakeOnPacketConnection(ip_string1, &e);
  EXPECT_EQ(e.type(), Error::kOperationFailed);
  EXPECT_STREQ(e.message().c_str(),
               "Max number of IP address patterns already registered");

  // Add and remove operations will still execute even when the wake on packet
  // feature has been disabled.
  SetWakeOnWiFiMaxPatterns(50);
  DisableWakeOnWiFiFeatures();
  GetWakeOnWiFiTriggersSupported()->insert(WakeOnWiFi::kPattern);
  AddWakeOnPacketConnection(ip_string1, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 1);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  AddWakeOnPacketConnection(ip_string2, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  RemoveWakeOnPacketConnection(ip_string1, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 1);
  RemoveAllWakeOnPacketConnections(&e);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Empty());

  // Normal functioning of add/remove operations when wake on WiFi features
  // are enabled, the NIC supports pattern matching, and the max number
  // of patterns have not been registered yet.
  EnableWakeOnWiFiFeaturesPacketSSID();
  GetWakeOnPacketConnections()->Clear();
  EXPECT_TRUE(GetWakeOnPacketConnections()->Empty());
  AddWakeOnPacketConnection(ip_string1, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 1);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  AddWakeOnPacketConnection(ip_string2, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  AddWakeOnPacketConnection(ip_string3, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 3);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  RemoveWakeOnPacketConnection(ip_string2, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  // Remove fails if no such address is registered.
  RemoveWakeOnPacketConnection(ip_string2, &e);
  EXPECT_EQ(e.type(), Error::kNotFound);
  EXPECT_STREQ(e.message().c_str(),
               "No such IP address match registered to wake device");
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);

  RemoveWakeOnPacketConnection(ip_string1, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 1);
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  AddWakeOnPacketConnection(ip_string2, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  RemoveAllWakeOnPacketConnections(&e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 0);
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr3));
}

TEST_F(WakeOnWiFiTestWithDispatcher, OnBeforeSuspend_ClearsEventHistory) {
  const int kNumEvents = WakeOnWiFi::kMaxDarkResumesPerPeriod - 1;
  for (int i = 0; i < kNumEvents; ++i) {
    GetDarkResumesSinceLastSuspend()->RecordEvent();
  }
  EXPECT_EQ(kNumEvents, GetDarkResumesSinceLastSuspend()->Size());
  OnBeforeSuspend(true, true, true, 0);
  EXPECT_TRUE(GetDarkResumesSinceLastSuspend()->Empty());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, OnBeforeSuspend_DHCPLeaseRenewal) {
  bool is_connected;
  bool have_dhcp_lease;
  const bool has_service_configured_for_autoconnect = true;
  // If we are connected the time to next lease renewal is short enough, we will
  // initiate DHCP lease renewal immediately.
  is_connected = true;
  have_dhcp_lease = true;
  EXPECT_CALL(*this, RenewDHCPLeaseCallback()).Times(1);
  EXPECT_CALL(mock_dispatcher_, PostTask(_)).Times(1);
  OnBeforeSuspend(is_connected, has_service_configured_for_autoconnect,
                  have_dhcp_lease, kTimeToNextLeaseRenewalShort);

  // No immediate DHCP lease renewal because we are not connected.
  is_connected = false;
  have_dhcp_lease = true;
  EXPECT_CALL(*this, RenewDHCPLeaseCallback()).Times(0);
  EXPECT_CALL(mock_dispatcher_, PostTask(_)).Times(1);
  OnBeforeSuspend(is_connected, has_service_configured_for_autoconnect,
                  have_dhcp_lease, kTimeToNextLeaseRenewalShort);

  // No immediate DHCP lease renewal because the time to the next lease renewal
  // is longer than the threshold.
  is_connected = true;
  have_dhcp_lease = true;
  EXPECT_CALL(*this, RenewDHCPLeaseCallback()).Times(0);
  EXPECT_CALL(mock_dispatcher_, PostTask(_)).Times(1);
  OnBeforeSuspend(is_connected, has_service_configured_for_autoconnect,
                  have_dhcp_lease, kTimeToNextLeaseRenewalLong);

  // No immediate DHCP lease renewal because we do not have a DHCP lease that
  // needs to be renewed.
  is_connected = true;
  have_dhcp_lease = false;
  EXPECT_CALL(*this, RenewDHCPLeaseCallback()).Times(0);
  EXPECT_CALL(mock_dispatcher_, PostTask(_)).Times(1);
  OnBeforeSuspend(is_connected, has_service_configured_for_autoconnect,
                  have_dhcp_lease, kTimeToNextLeaseRenewalLong);
}

TEST_F(WakeOnWiFiTestWithDispatcher, OnDarkResume_Connected_Timeout) {
  // Test that correct actions are taken if we enter OnDarkResume while
  // connected in dark resume.
  const bool is_connected = true;
  const bool has_service_configured_for_autoconnect = true;
  InitStateForDarkResume();
  EXPECT_TRUE(DarkResumeActionsTimeOutCallbackIsCancelled());
  // Renew DHCP lease if we are connected in dark resume.
  EXPECT_CALL(*this, RenewDHCPLeaseCallback());
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);
  EXPECT_FALSE(DarkResumeActionsTimeOutCallbackIsCancelled());
  // Trigger timeout callback.
  // Since we timeout, we are disconnected before suspend.
  StartDHCPLeaseRenewalTimer();
  StopWakeToScanTimer();
  SetExpectationsDisconnectedBeforeSuspend();
  dispatcher_.DispatchPendingEvents();
  EXPECT_FALSE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_TRUE(WakeToScanTimerIsRunning());
  VerifyStateDisconnectedBeforeSuspend();
}

TEST_F(WakeOnWiFiTestWithDispatcher, OnDarkResume_Connected_LeaseObtained) {
  // Test that correct actions are taken if we enter OnDarkResume while
  // connected in dark resume.
  const bool is_connected = true;
  const bool have_dhcp_lease = true;
  const uint32_t time_to_next_lease_renewal = 10;
  const bool has_service_configured_for_autoconnect = true;
  InitStateForDarkResume();
  EXPECT_TRUE(DarkResumeActionsTimeOutCallbackIsCancelled());
  // Renew DHCP lease if we are connected in dark resume.
  EXPECT_CALL(*this, RenewDHCPLeaseCallback());
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);
  EXPECT_FALSE(DarkResumeActionsTimeOutCallbackIsCancelled());
  // Lease obtained.
  // Since a lease is obtained, we are connected before suspend.
  StopDHCPLeaseRenewalTimer();
  StartWakeToScanTimer();
  SetExpectationsConnectedBeforeSuspend();
  OnDHCPLeaseObtained(have_dhcp_lease, time_to_next_lease_renewal);
  EXPECT_TRUE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_FALSE(WakeToScanTimerIsRunning());
  VerifyStateConnectedBeforeSuspend();
}

TEST_F(WakeOnWiFiTestWithDispatcher, OnDarkResume_Connected_DoNotRecordEvent) {
  const bool is_connected = true;
  const bool has_service_configured_for_autoconnect = true;
  EXPECT_TRUE(GetDarkResumesSinceLastSuspend()->Empty());
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);
  EXPECT_TRUE(GetDarkResumesSinceLastSuspend()->Empty());
}

TEST_F(WakeOnWiFiTestWithDispatcher, OnDarkResume_NotConnected_Timeout) {
  // Test that correct actions are taken if we enter OnDarkResume while
  // not connected in dark resume.
  const bool is_connected = false;
  const bool has_service_configured_for_autoconnect = true;
  InitStateForDarkResume();
  EXPECT_TRUE(DarkResumeActionsTimeOutCallbackIsCancelled());
  // Initiate scan if we are not connected in dark resume.
  EXPECT_CALL(*this, RemoveSupplicantNetworksCallback());
  EXPECT_CALL(*this, InitiateScanCallback());
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);
  EXPECT_FALSE(DarkResumeActionsTimeOutCallbackIsCancelled());
  // Trigger timeout callback.
  // Since we timeout, we are disconnected before suspend.
  StartDHCPLeaseRenewalTimer();
  StopWakeToScanTimer();
  SetExpectationsDisconnectedBeforeSuspend();
  dispatcher_.DispatchPendingEvents();
  EXPECT_FALSE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_TRUE(WakeToScanTimerIsRunning());
  VerifyStateDisconnectedBeforeSuspend();
}

TEST_F(WakeOnWiFiTestWithDispatcher, OnDarkResume_NotConnected_LeaseObtained) {
  // Test that correct actions are taken if we enter OnDarkResume while
  // not connected in dark resume.
  const bool is_connected = false;
  const bool have_dhcp_lease = true;
  const uint32_t time_to_next_lease_renewal = 10;
  const bool has_service_configured_for_autoconnect = true;
  InitStateForDarkResume();
  EXPECT_TRUE(DarkResumeActionsTimeOutCallbackIsCancelled());
  // Initiate scan if we are not connected in dark resume.
  EXPECT_CALL(*this, RemoveSupplicantNetworksCallback());
  EXPECT_CALL(*this, InitiateScanCallback());
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);
  EXPECT_FALSE(DarkResumeActionsTimeOutCallbackIsCancelled());
  // Lease obtained.
  // Since a lease is obtained, we are connected before suspend.
  StopDHCPLeaseRenewalTimer();
  StartWakeToScanTimer();
  SetExpectationsConnectedBeforeSuspend();
  OnDHCPLeaseObtained(have_dhcp_lease, time_to_next_lease_renewal);
  EXPECT_TRUE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_FALSE(WakeToScanTimerIsRunning());
  VerifyStateConnectedBeforeSuspend();
}

TEST_F(WakeOnWiFiTestWithDispatcher, OnDarkResume_NotConnected_RecordEvent) {
  const bool is_connected = false;
  const bool has_service_configured_for_autoconnect = true;
  EXPECT_TRUE(GetDarkResumesSinceLastSuspend()->Empty());
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);
  EXPECT_EQ(1, GetDarkResumesSinceLastSuspend()->Size());
}

TEST_F(WakeOnWiFiTestWithDispatcher, OnDarkResume_NotConnected_Throttle) {
  const bool is_connected = false;
  bool has_service_configured_for_autoconnect = true;
  EXPECT_TRUE(GetDarkResumesSinceLastSuspend()->Empty());
  for (int i = 0; i < WakeOnWiFi::kMaxDarkResumesPerPeriod - 1; ++i) {
    OnDarkResume(is_connected, has_service_configured_for_autoconnect);
  }
  EXPECT_EQ(WakeOnWiFi::kMaxDarkResumesPerPeriod - 1,
            GetDarkResumesSinceLastSuspend()->Size());

  // Max dark resumes per period reached, so disable wake on WiFi and stop all
  // RTC timers.
  SetInDarkResume(false);
  ResetSuspendActionsDoneCallback();
  StartDHCPLeaseRenewalTimer();
  StartWakeToScanTimer();
  EXPECT_TRUE(SuspendActionsCallbackIsNull());
  EXPECT_TRUE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_TRUE(WakeToScanTimerIsRunning());
  EXPECT_FALSE(GetDarkResumesSinceLastSuspend()->Empty());
  EXPECT_CALL(metrics_, NotifyWakeOnWiFiThrottled());
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);
  EXPECT_FALSE(SuspendActionsCallbackIsNull());
  EXPECT_FALSE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_FALSE(WakeToScanTimerIsRunning());
  EXPECT_TRUE(GetDarkResumesSinceLastSuspend()->Empty());
  EXPECT_FALSE(GetInDarkResume());
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, OnDHCPLeaseObtained) {
  const bool start_lease_renewal_timer = true;
  ScopedMockLog log;

  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  SetInDarkResume(true);
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(3);
  EXPECT_CALL(log, Log(_, _, HasSubstr("BeforeSuspendActions")));
  OnDHCPLeaseObtained(start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);

  SetInDarkResume(false);
  EXPECT_CALL(log, Log(_, _, HasSubstr("Not in dark resume, so do nothing")));
  OnDHCPLeaseObtained(start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  ScopeLogger::GetInstance()->set_verbose_level(0);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, WakeOnWiFiDisabledAfterResume) {
  // At least one wake on WiFi trigger supported and Wake on WiFi features
  // are enabled, so disable Wake on WiFi on resume.
  EnableWakeOnWiFiFeaturesPacketSSID();
  GetWakeOnWiFiTriggers()->insert(WakeOnWiFi::kPattern);
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnWiFiMsg(), _, _, _)).Times(1);
  EXPECT_CALL(metrics_, NotifySuspendWithWakeOnWiFiEnabledDone()).Times(1);
  OnAfterResume();

  // No wake no WiFi triggers supported, so do nothing.
  ClearWakeOnWiFiTriggersSupported();
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnWiFiMsg(), _, _, _)).Times(0);
  EXPECT_CALL(metrics_, NotifySuspendWithWakeOnWiFiEnabledDone()).Times(0);
  OnAfterResume();

  // Wake on WiFi features disabled, so do nothing.
  GetWakeOnWiFiTriggersSupported()->insert(WakeOnWiFi::kPattern);
  DisableWakeOnWiFiFeatures();
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnWiFiMsg(), _, _, _)).Times(0);
  EXPECT_CALL(metrics_, NotifySuspendWithWakeOnWiFiEnabledDone()).Times(0);
  OnAfterResume();

  // Both WakeOnWiFi triggers are empty and Wake on WiFi features are disabled,
  // so do nothing.
  ClearWakeOnWiFiTriggersSupported();
  DisableWakeOnWiFiFeatures();
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnWiFiMsg(), _, _, _)).Times(0);
  EXPECT_CALL(metrics_, NotifySuspendWithWakeOnWiFiEnabledDone()).Times(0);
  OnAfterResume();
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, SetWakeOnWiFiFeaturesEnabled) {
  const string bad_feature("blahblah");
  Error e;
  EnableWakeOnWiFiFeaturesPacketSSID();
  EXPECT_STREQ(GetWakeOnWiFiFeaturesEnabled().c_str(),
               kWakeOnWiFiFeaturesEnabledPacketSSID);
  EXPECT_FALSE(
      SetWakeOnWiFiFeaturesEnabled(kWakeOnWiFiFeaturesEnabledPacketSSID, &e));
  EXPECT_STREQ(GetWakeOnWiFiFeaturesEnabled().c_str(),
               kWakeOnWiFiFeaturesEnabledPacketSSID);

  EXPECT_FALSE(SetWakeOnWiFiFeaturesEnabled(bad_feature, &e));
  EXPECT_EQ(e.type(), Error::kInvalidArguments);
  EXPECT_STREQ(e.message().c_str(), "Invalid Wake on WiFi feature");
  EXPECT_STREQ(GetWakeOnWiFiFeaturesEnabled().c_str(),
               kWakeOnWiFiFeaturesEnabledPacketSSID);

  EXPECT_TRUE(
      SetWakeOnWiFiFeaturesEnabled(kWakeOnWiFiFeaturesEnabledPacket, &e));
  EXPECT_STREQ(GetWakeOnWiFiFeaturesEnabled().c_str(),
               kWakeOnWiFiFeaturesEnabledPacket);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ReportConnectedToServiceAfterWake_WakeOnSSIDEnabledAndConnected) {
  const bool is_connected = true;
  EnableWakeOnWiFiFeaturesPacketSSID();
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::kWiFiConnetionStatusAfterWakeOnWiFiEnabledWakeConnected));
  ReportConnectedToServiceAfterWake(is_connected);

  EnableWakeOnWiFiFeaturesSSID();
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::kWiFiConnetionStatusAfterWakeOnWiFiEnabledWakeConnected));
  ReportConnectedToServiceAfterWake(is_connected);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ReportConnectedToServiceAfterWake_WakeOnSSIDEnabledAndNotConnected) {
  const bool is_connected = false;
  EnableWakeOnWiFiFeaturesPacketSSID();
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::kWiFiConnetionStatusAfterWakeOnWiFiEnabledWakeNotConnected));
  ReportConnectedToServiceAfterWake(is_connected);

  EnableWakeOnWiFiFeaturesSSID();
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::kWiFiConnetionStatusAfterWakeOnWiFiEnabledWakeNotConnected));
  ReportConnectedToServiceAfterWake(is_connected);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ReportConnectedToServiceAfterWake_WakeOnSSIDDisabledAndConnected) {
  const bool is_connected = true;
  EnableWakeOnWiFiFeaturesPacket();
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeConnected));
  ReportConnectedToServiceAfterWake(is_connected);

  DisableWakeOnWiFiFeatures();
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeConnected));
  ReportConnectedToServiceAfterWake(is_connected);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       ReportConnectedToServiceAfterWake_WakeOnSSIDDisabledAndNotConnected) {
  const bool is_connected = false;
  EnableWakeOnWiFiFeaturesPacket();
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::
              kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeNotConnected));
  ReportConnectedToServiceAfterWake(is_connected);

  DisableWakeOnWiFiFeatures();
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::
              kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeNotConnected));
  ReportConnectedToServiceAfterWake(is_connected);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher, OnNoAutoConnectableServicesAfterScan) {
  const bool has_service_configured_for_autoconnect = true;
  const Closure remove_supplicant_networks_callback(Bind(
      &WakeOnWiFiTest::RemoveSupplicantNetworksCallback, Unretained(this)));

  // Perform disconnect before suspend actions if we are in dark resume.
  SetInDarkResume(true);
  EnableWakeOnWiFiFeaturesSSID();
  GetWakeOnWiFiTriggers()->clear();
  StartDHCPLeaseRenewalTimer();
  StopWakeToScanTimer();
  OnNoAutoConnectableServicesAfterScan(has_service_configured_for_autoconnect,
                                       remove_supplicant_networks_callback);
  EXPECT_TRUE(WakeToScanTimerIsRunning());
  EXPECT_FALSE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 1);
  EXPECT_TRUE(GetWakeOnWiFiTriggers()->find(WakeOnWiFi::kSSID) !=
              GetWakeOnWiFiTriggers()->end());

  // Otherwise, do not call WakeOnWiFi::BeforeSuspendActions and do nothing.
  SetInDarkResume(false);
  GetWakeOnWiFiTriggers()->clear();
  StartDHCPLeaseRenewalTimer();
  StopWakeToScanTimer();
  OnNoAutoConnectableServicesAfterScan(has_service_configured_for_autoconnect,
                                       remove_supplicant_networks_callback);
  EXPECT_FALSE(WakeToScanTimerIsRunning());
  EXPECT_TRUE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 0);
}

#else

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_AddWakeOnPacketConnection_ReturnsError) {
  DisableWakeOnWiFiFeatures();
  Error e;
  AddWakeOnPacketConnection("1.1.1.1", &e);
  EXPECT_EQ(e.type(), Error::kNotSupported);
  EXPECT_STREQ(e.message().c_str(), WakeOnWiFi::kWakeOnWiFiDisabled);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_RemoveWakeOnPacketConnection_ReturnsError) {
  DisableWakeOnWiFiFeatures();
  Error e;
  RemoveWakeOnPacketConnection("1.1.1.1", &e);
  EXPECT_EQ(e.type(), Error::kNotSupported);
  EXPECT_STREQ(e.message().c_str(), WakeOnWiFi::kWakeOnWiFiDisabled);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_RemoveAllWakeOnPacketConnections_ReturnsError) {
  DisableWakeOnWiFiFeatures();
  Error e;
  RemoveAllWakeOnPacketConnections(&e);
  EXPECT_EQ(e.type(), Error::kNotSupported);
  EXPECT_STREQ(e.message().c_str(), WakeOnWiFi::kWakeOnWiFiDisabled);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_OnBeforeSuspend_ReportsDoneImmediately) {
  const bool is_connected = true;
  const bool have_dhcp_lease = true;
  const bool has_service_configured_for_autoconnect = true;
  EXPECT_CALL(*this, DoneCallback(ErrorType(Error::kSuccess))).Times(1);
  EXPECT_CALL(*this, RenewDHCPLeaseCallback()).Times(0);
  OnBeforeSuspend(is_connected, has_service_configured_for_autoconnect,
                  have_dhcp_lease, kTimeToNextLeaseRenewalShort);

  EXPECT_CALL(*this, DoneCallback(ErrorType(Error::kSuccess))).Times(1);
  EXPECT_CALL(*this, RenewDHCPLeaseCallback()).Times(0);
  OnBeforeSuspend(is_connected, has_service_configured_for_autoconnect,
                  have_dhcp_lease, kTimeToNextLeaseRenewalLong);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_OnDarkResume_ReportsDoneImmediately) {
  const bool is_connected = true;
  const bool has_service_configured_for_autoconnect = true;
  EXPECT_CALL(*this, DoneCallback(ErrorType(Error::kSuccess))).Times(1);
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(_, _)).Times(0);
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);

  EXPECT_CALL(*this, DoneCallback(ErrorType(Error::kSuccess))).Times(1);
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(_, _)).Times(0);
  OnDarkResume(is_connected, has_service_configured_for_autoconnect);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_OnAfterResume_DoesNothing) {
  DisableWakeOnWiFiFeatures();
  EXPECT_CALL(netlink_manager_, SendNl80211Message(_, _, _, _)).Times(0);
  EXPECT_CALL(metrics_, NotifySuspendWithWakeOnWiFiEnabledDone()).Times(0);
  OnAfterResume();
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_SetWakeOnWiFiFeaturesEnabled) {
  Error e;
  SetWakeOnWiFiFeaturesNotSupported();
  EXPECT_STREQ(GetWakeOnWiFiFeaturesEnabled().c_str(),
               kWakeOnWiFiFeaturesEnabledNotSupported);
  EXPECT_FALSE(
      SetWakeOnWiFiFeaturesEnabled(kWakeOnWiFiFeaturesEnabledNotSupported, &e));
  EXPECT_STREQ(GetWakeOnWiFiFeaturesEnabled().c_str(),
               kWakeOnWiFiFeaturesEnabledNotSupported);
  EXPECT_EQ(e.type(), Error::kNotSupported);
  EXPECT_STREQ(e.message().c_str(), "Wake on WiFi is not supported");

  EXPECT_FALSE(
      SetWakeOnWiFiFeaturesEnabled(kWakeOnWiFiFeaturesEnabledPacket, &e));
  EXPECT_STREQ(GetWakeOnWiFiFeaturesEnabled().c_str(),
               kWakeOnWiFiFeaturesEnabledNotSupported);
  EXPECT_EQ(e.type(), Error::kNotSupported);
  EXPECT_STREQ(e.message().c_str(), "Wake on WiFi is not supported");
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_OnDHCPLeaseObtained) {
  ScopedMockLog log;
  const bool start_lease_renewal_timer = true;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(2);

  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  SetInDarkResume(true);
  EXPECT_CALL(
      log, Log(_, _, HasSubstr("Wake on WiFi not supported, so do nothing")));
  OnDHCPLeaseObtained(start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);

  SetInDarkResume(false);
  EXPECT_CALL(log, Log(_, _, HasSubstr("Not in dark resume, so do nothing")));
  OnDHCPLeaseObtained(start_lease_renewal_timer, kTimeToNextLeaseRenewalLong);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  ScopeLogger::GetInstance()->set_verbose_level(0);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_ReportConnectedToServiceAfterWakeAndConnected) {
  const bool is_connected = true;
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeConnected));
  ReportConnectedToServiceAfterWake(is_connected);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_ReportConnectedToServiceAfterWakeAndNotConnected) {
  const bool is_connected = false;
  EXPECT_CALL(
      metrics_,
      NotifyConnectedToServiceAfterWake(
          Metrics::
              kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeNotConnected));
  ReportConnectedToServiceAfterWake(is_connected);
}

TEST_F(WakeOnWiFiTestWithMockDispatcher,
       WakeOnWiFiDisabled_OnNoAutoConnectableServicesAfterScan) {
  const bool has_service_configured_for_autoconnect = true;
  const Closure remove_supplicant_networks_callback(Bind(
      &WakeOnWiFiTest::RemoveSupplicantNetworksCallback, Unretained(this)));

  // Do nothing (i.e. do not invoke WakeOnWiFi::BeforeSuspendActions) if wake
  // on WiFi is not supported, whether or not we are in dark resume.
  SetInDarkResume(true);
  GetWakeOnWiFiTriggers()->clear();
  StartDHCPLeaseRenewalTimer();
  StopWakeToScanTimer();
  OnNoAutoConnectableServicesAfterScan(has_service_configured_for_autoconnect,
                                       remove_supplicant_networks_callback);
  EXPECT_FALSE(WakeToScanTimerIsRunning());
  EXPECT_TRUE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 0);

  SetInDarkResume(false);
  GetWakeOnWiFiTriggers()->clear();
  StartDHCPLeaseRenewalTimer();
  StopWakeToScanTimer();
  OnNoAutoConnectableServicesAfterScan(has_service_configured_for_autoconnect,
                                       remove_supplicant_networks_callback);
  EXPECT_FALSE(WakeToScanTimerIsRunning());
  EXPECT_TRUE(DHCPLeaseRenewalTimerIsRunning());
  EXPECT_EQ(GetWakeOnWiFiTriggers()->size(), 0);
}

#endif  // DISABLE_WAKE_ON_WIFI

}  // namespace shill
