// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains functions that shouldn't be used outside of util.cc
// and util_test.cc primarily to abstract libusbguard related types and symbols,
// but still allow for them to be used in unit tests.

#ifndef USB_BOUNCER_UTIL_INTERNAL_H_
#define USB_BOUNCER_UTIL_INTERNAL_H_

#include "usb_bouncer/util.h"

#include <string>

#include <usbguard/Rule.hpp>

namespace usb_bouncer {

enum class UMADeviceClass {
  kApp,
  kAudio,
  kAV,
  kCard,
  kComm,
  kHealth,
  kHID,
  kHub,
  kImage,
  kMisc,
  kOther,
  kPhys,
  kPrint,
  kSec,
  kStorage,
  kVendor,
  kVideo,
  kWireless,
};

const std::string to_string(UMADeviceClass device_class);
const std::string to_string(UMADeviceRecognized recognized);
std::ostream& operator<<(std::ostream& out, UMADeviceClass device_class);
std::ostream& operator<<(std::ostream& out, UMADeviceRecognized recognized);

// libusbguard uses exceptions, so this converts the exception case to a return
// value that tests as bool false.
usbguard::Rule GetRuleFromString(const std::string& to_parse);

UMADeviceClass GetClassFromRule(const usbguard::Rule& rule);

}  // namespace usb_bouncer

#endif  // USB_BOUNCER_UTIL_INTERNAL_H_
