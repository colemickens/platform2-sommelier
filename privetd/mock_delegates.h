// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_MOCK_DELEGATES_H_
#define PRIVETD_MOCK_DELEGATES_H_

#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/identity_delegate.h"
#include "privetd/security_delegate.h"
#include "privetd/wifi_delegate.h"

using testing::_;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgPointee;

namespace privetd {

class MockDeviceDelegate : public DeviceDelegate {
  using IntPair = std::pair<uint16_t, uint16_t>;

 public:
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_CONST_METHOD0(GetDescription, std::string());
  MOCK_CONST_METHOD0(GetLocation, std::string());
  MOCK_CONST_METHOD0(GetClass, std::string());
  MOCK_CONST_METHOD0(GetModelId, std::string());
  MOCK_CONST_METHOD0(GetServices, std::vector<std::string>());
  MOCK_CONST_METHOD0(GetHttpEnpoint, IntPair());
  MOCK_CONST_METHOD0(GetHttpsEnpoint, IntPair());
  MOCK_CONST_METHOD0(GetUptime, base::TimeDelta());
  MOCK_METHOD1(SetName, void(const std::string&));
  MOCK_METHOD1(SetDescription, void(const std::string&));
  MOCK_METHOD1(SetLocation, void(const std::string&));
  MOCK_METHOD1(SetHttpPort, void(uint16_t));
  MOCK_METHOD1(SetHttpsPort, void(uint16_t));

  MockDeviceDelegate() {
    EXPECT_CALL(*this, GetName()).WillRepeatedly(Return("TestDevice"));
    EXPECT_CALL(*this, GetDescription()).WillRepeatedly(Return(""));
    EXPECT_CALL(*this, GetLocation()).WillRepeatedly(Return(""));
    EXPECT_CALL(*this, GetClass()).WillRepeatedly(Return("AB"));
    EXPECT_CALL(*this, GetModelId()).WillRepeatedly(Return("MID"));
    EXPECT_CALL(*this, GetServices())
        .WillRepeatedly(Return(std::vector<std::string>{}));
    EXPECT_CALL(*this, GetHttpEnpoint())
        .WillRepeatedly(Return(std::make_pair(0, 0)));
    EXPECT_CALL(*this, GetHttpsEnpoint())
        .WillRepeatedly(Return(std::make_pair(0, 0)));
    EXPECT_CALL(*this, GetUptime())
        .WillRepeatedly(Return(base::TimeDelta::FromHours(1)));
  }
};

class MockSecurityDelegate : public SecurityDelegate {
 public:
  MOCK_CONST_METHOD2(CreateAccessToken,
                     std::string(AuthScope, const base::Time&));
  MOCK_CONST_METHOD2(ParseAccessToken,
                     AuthScope(const std::string&, base::Time*));
  MOCK_CONST_METHOD0(GetPairingTypes, std::vector<PairingType>());
  MOCK_CONST_METHOD0(GetCryptoTypes, std::vector<CryptoType>());
  MOCK_CONST_METHOD1(IsValidPairingCode, bool(const std::string&));
  MOCK_METHOD5(StartPairing,
               bool(PairingType,
                    CryptoType,
                    std::string*,
                    std::string*,
                    chromeos::ErrorPtr*));
  MOCK_METHOD5(ConfirmPairing,
               bool(const std::string&,
                    const std::string&,
                    std::string*,
                    std::string*,
                    chromeos::ErrorPtr*));
  MOCK_METHOD2(CancelPairing, bool(const std::string&, chromeos::ErrorPtr*));

  MockSecurityDelegate() {
    EXPECT_CALL(*this, CreateAccessToken(_, _))
        .WillRepeatedly(Return("GuestAccessToken"));

    EXPECT_CALL(*this, ParseAccessToken(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(base::Time::Now()),
                              Return(AuthScope::kGuest)));

    EXPECT_CALL(*this, GetPairingTypes())
        .WillRepeatedly(Return(std::vector<PairingType>{
            PairingType::kPinCode,
            PairingType::kEmbeddedCode,
            PairingType::kUltrasoundDsssBroadcaster,
            PairingType::kAudibleDtmfBroadcaster,
        }));

    EXPECT_CALL(*this, GetCryptoTypes())
        .WillRepeatedly(Return(std::vector<CryptoType>{
            CryptoType::kSpake_p224, CryptoType::kSpake_p256,
        }));

    EXPECT_CALL(*this, StartPairing(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>("testSession"),
                              SetArgPointee<3>("testCommitment"),
                              Return(true)));

    EXPECT_CALL(*this, ConfirmPairing(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>("testFingerprint"),
                              SetArgPointee<3>("testSignature"), Return(true)));
    EXPECT_CALL(*this, CancelPairing(_, _)).WillRepeatedly(Return(true));
  }
};

class MockWifiDelegate : public WifiDelegate {
 public:
  MOCK_CONST_METHOD0(GetConnectionState, const ConnectionState&());
  MOCK_CONST_METHOD0(GetSetupState, const SetupState&());
  MOCK_METHOD3(ConfigureCredentials,
               bool(const std::string&,
                    const std::string&,
                    chromeos::ErrorPtr*));
  MOCK_CONST_METHOD0(GetCurrentlyConnectedSsid, std::string());
  MOCK_CONST_METHOD0(GetHostedSsid, std::string());
  MOCK_CONST_METHOD0(GetTypes, std::vector<WifiType>());

  MockWifiDelegate() {
    EXPECT_CALL(*this, GetConnectionState())
        .WillRepeatedly(ReturnRef(connection_state_));
    EXPECT_CALL(*this, GetSetupState()).WillRepeatedly(ReturnRef(setup_state_));
    EXPECT_CALL(*this, GetCurrentlyConnectedSsid())
        .WillRepeatedly(Return("TestSsid"));
    EXPECT_CALL(*this, GetHostedSsid())
        .WillRepeatedly(Return("Test_device.BBABCLAprv"));
    EXPECT_CALL(*this, GetTypes())
        .WillRepeatedly(Return(std::vector<WifiType>{WifiType::kWifi24}));
  }

  ConnectionState connection_state_{ConnectionState::kOffline};
  SetupState setup_state_{SetupState::kNone};
};

class MockCloudDelegate : public CloudDelegate {
 public:
  MOCK_CONST_METHOD0(GetConnectionState, const ConnectionState&());
  MOCK_CONST_METHOD0(GetSetupState, const SetupState&());
  MOCK_METHOD3(Setup,
               bool(const std::string&,
                    const std::string&,
                    chromeos::ErrorPtr*));
  MOCK_CONST_METHOD0(GetCloudId, std::string());

  MockCloudDelegate() {
    EXPECT_CALL(*this, GetConnectionState())
        .WillRepeatedly(ReturnRef(connection_state_));
    EXPECT_CALL(*this, GetSetupState()).WillRepeatedly(ReturnRef(setup_state_));
    EXPECT_CALL(*this, GetCloudId()).WillRepeatedly(Return("TestCloudId"));
  }

  ConnectionState connection_state_{ConnectionState::kOnline};
  SetupState setup_state_{SetupState::kNone};
};

class MockIdentityDelegate : public IdentityDelegate {
 public:
  MOCK_CONST_METHOD0(GetId, std::string());

  MockIdentityDelegate() {
    EXPECT_CALL(*this, GetId()).WillRepeatedly(Return("TestId"));
  }
};

}  // namespace privetd

#endif  // PRIVETD_MOCK_DELEGATES_H_
