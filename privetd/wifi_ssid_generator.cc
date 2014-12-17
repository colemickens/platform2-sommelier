// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/wifi_ssid_generator.h"

#include <bitset>

#include <base/bind.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/wifi_delegate.h"

namespace privetd {

namespace {

const int kDeviceNameSize = 20;
// [DeviceName+Idx <= 20].[class == 2][modelID == 3][flags == 2]prv
const char kSsidFormat[] = "%s %s.%2.2s%3.3s%2.2sprv";

const char base64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool IsSetupNeeded(ConnectionState::Status status) {
  switch (status) {
    case ConnectionState::kUnconfigured:
    case ConnectionState::kError:
      return true;
    case ConnectionState::kDisabled:
    case ConnectionState::kConnecting:
    case ConnectionState::kOnline:
    case ConnectionState::kOffline:
      return false;
  }
  CHECK(false);
  return false;
}

}  // namespace

WifiSsidGenerator::WifiSsidGenerator(const DeviceDelegate* device,
                                     const CloudDelegate* cloud,
                                     const WifiDelegate* wifi)
    : device_(device),
      gcd_(cloud),
      wifi_(wifi),
      get_random_(base::Bind(&base::RandInt, 0, 99)) {
}

std::string WifiSsidGenerator::GenerateFlags() const {
  std::bitset<6> flags1;
  // Device needs WiFi configuration.
  flags1[0] = wifi_ && IsSetupNeeded(wifi_->GetConnectionState().status);
  // Device needs GCD registration.
  flags1[1] = gcd_ && IsSetupNeeded(gcd_->GetConnectionState().status);

  std::bitset<6> flags2;
  // Device is discoverable over WiFi.
  flags2[0] = true;

  std::string result{2, base64chars[0]};
  result[0] = base64chars[flags1.to_ulong()];
  result[1] = base64chars[flags2.to_ulong()];
  return result;
}

std::string WifiSsidGenerator::GenerateSsid() const {
  std::string idx{base::IntToString(get_random_.Run())};

  std::string name{
      device_->GetName().substr(0, kDeviceNameSize - idx.size() - 1)};

  std::string dev_class{device_->GetClass()};
  CHECK_EQ(2u, dev_class.size());

  std::string model_id{device_->GetModelId()};
  CHECK_EQ(3u, model_id.size());

  std::string result = base::StringPrintf(
      kSsidFormat, name.c_str(), idx.c_str(), dev_class.c_str(),
      model_id.c_str(), GenerateFlags().c_str());
  CHECK_EQ(result[result.size() - 11], '.');
  return result;
}

void WifiSsidGenerator::SetRandomForTests(int n) {
  get_random_ = base::Bind(&base::RandInt, n, n);
}

}  // namespace privetd
