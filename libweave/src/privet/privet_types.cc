// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/privet_types.h"

#include <string>

#include "weave/enum_to_string.h"
#include "weave/types.h"

namespace weave {

namespace {

using privet::AuthScope;
using privet::ConnectionState;
using privet::CryptoType;
using privet::SetupState;
using privet::WifiType;

const EnumToStringMap<PairingType>::Map kPairingTypeMap[] = {
    {PairingType::kPinCode, "pinCode"},
    {PairingType::kEmbeddedCode, "embeddedCode"},
    {PairingType::kUltrasound32, "ultrasound32"},
    {PairingType::kAudible32, "audible32"},
};

const EnumToStringMap<ConnectionState::Status>::Map kConnectionStateMap[] = {
    {ConnectionState::kDisabled, "disabled"},
    {ConnectionState::kUnconfigured, "unconfigured"},
    {ConnectionState::kConnecting, "connecting"},
    {ConnectionState::kOnline, "online"},
    {ConnectionState::kOffline, "offline"},
};

const EnumToStringMap<SetupState::Status>::Map kSetupStateMap[] = {
    {SetupState::kNone, nullptr},
    {SetupState::kInProgress, "inProgress"},
    {SetupState::kSuccess, "success"},
};

const EnumToStringMap<WifiType>::Map kWifiTypeMap[] = {
    {WifiType::kWifi24, "2.4GHz"},
    {WifiType::kWifi50, "5.0GHz"},
};

const EnumToStringMap<CryptoType>::Map kCryptoTypeMap[] = {
    {CryptoType::kNone, "none"},
    {CryptoType::kSpake_p224, "p224_spake2"},
    {CryptoType::kSpake_p256, "p256_spake2"},
};

const EnumToStringMap<AuthScope>::Map kAuthScopeMap[] = {
    {AuthScope::kNone, "none"},
    {AuthScope::kViewer, "viewer"},
    {AuthScope::kUser, "user"},
    {AuthScope::kOwner, "owner"},
};

const EnumToStringMap<WifiSetupState>::Map kWifiSetupStateMap[] = {
    {WifiSetupState::kDisabled, "disabled"},
    {WifiSetupState::kBootstrapping, "waiting"},
    {WifiSetupState::kMonitoring, "monitoring"},
    {WifiSetupState::kConnecting, "connecting"},
};

}  // namespace

template <>
EnumToStringMap<PairingType>::EnumToStringMap()
    : EnumToStringMap(kPairingTypeMap) {}

template <>
EnumToStringMap<ConnectionState::Status>::EnumToStringMap()
    : EnumToStringMap(kConnectionStateMap) {}

template <>
EnumToStringMap<SetupState::Status>::EnumToStringMap()
    : EnumToStringMap(kSetupStateMap) {}

template <>
EnumToStringMap<WifiType>::EnumToStringMap()
    : EnumToStringMap(kWifiTypeMap) {}

template <>
EnumToStringMap<CryptoType>::EnumToStringMap()
    : EnumToStringMap(kCryptoTypeMap) {}

template <>
EnumToStringMap<AuthScope>::EnumToStringMap()
    : EnumToStringMap(kAuthScopeMap) {}

template <>
EnumToStringMap<WifiSetupState>::EnumToStringMap()
    : EnumToStringMap(kWifiSetupStateMap) {}

}  // namespace weave
