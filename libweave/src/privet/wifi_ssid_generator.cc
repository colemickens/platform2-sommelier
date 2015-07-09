// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/wifi_ssid_generator.h"

#include <bitset>

#include <base/bind.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

#include "libweave/src/privet/cloud_delegate.h"
#include "libweave/src/privet/device_delegate.h"
#include "libweave/src/privet/wifi_delegate.h"

namespace weave {
namespace privet {

namespace {

const int kDeviceNameSize = 20;
// [DeviceName+Idx <= 20].[modelID == 5][flags == 2]prv
const char kSsidFormat[] = "%s %s.%5.5s%2.2sprv";

const char base64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool IsSetupNeeded(const ConnectionState& state) {
  if (state.error())
    return true;
  switch (state.status()) {
    case ConnectionState::kUnconfigured:
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

WifiSsidGenerator::WifiSsidGenerator(const CloudDelegate* cloud,
                                     const WifiDelegate* wifi)
    : gcd_(cloud), wifi_(wifi), get_random_(base::Bind(&base::RandInt, 0, 99)) {
  CHECK(gcd_);
}

std::string WifiSsidGenerator::GenerateFlags() const {
  std::bitset<6> flags1;
  // Device needs WiFi configuration.
  flags1[0] = wifi_ && IsSetupNeeded(wifi_->GetConnectionState());
  // Device needs GCD registration.
  flags1[1] = IsSetupNeeded(gcd_->GetConnectionState());

  std::bitset<6> flags2;
  // Device is discoverable over WiFi.
  flags2[0] = true;

  std::string result{2, base64chars[0]};
  result[0] = base64chars[flags1.to_ulong()];
  result[1] = base64chars[flags2.to_ulong()];
  return result;
}

std::string WifiSsidGenerator::GenerateSsid() const {
  std::string name;
  std::string model_id;
  if (!gcd_ || !gcd_->GetName(&name, nullptr) ||
      !gcd_->GetModelId(&model_id, nullptr)) {
    return std::string();
  }
  std::string idx{base::IntToString(get_random_.Run())};
  name = name.substr(0, kDeviceNameSize - idx.size() - 1);
  CHECK_EQ(5u, model_id.size());

  std::string result =
      base::StringPrintf(kSsidFormat, name.c_str(), idx.c_str(),
                         model_id.c_str(), GenerateFlags().c_str());
  CHECK_EQ(result[result.size() - 11], '.');
  return result;
}

void WifiSsidGenerator::SetRandomForTests(int n) {
  get_random_ = base::Bind(&base::RandInt, n, n);
}

}  // namespace privet
}  // namespace weave
