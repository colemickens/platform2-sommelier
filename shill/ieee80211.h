// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IEEE80211_H
#define SHILL_IEEE80211_H

namespace shill {

namespace IEEE_80211 {
const uint8_t kElemIdErp = 42;
const uint8_t kElemIdHTCap = 45;
const uint8_t kElemIdHTInfo = 61;
const uint8_t kElemIdVendor = 221;

const unsigned int kMaxSSIDLen = 32;

const unsigned int kWEP40AsciiLen = 5;
const unsigned int kWEP40HexLen = 10;
const unsigned int kWEP104AsciiLen = 13;
const unsigned int kWEP104HexLen = 26;

const unsigned int kWPAAsciiMinLen = 8;
const unsigned int kWPAAsciiMaxLen = 63;
const unsigned int kWPAHexLen = 64;

const uint32_t kOUIVendorEpigram = 0x00904c;
const uint32_t kOUIVendorMicrosoft = 0x0050f2;

const uint8_t kOUIMicrosoftWPS = 4;
const uint16_t kWPSElementManufacturer = 0x1021;
const uint16_t kWPSElementModelName = 0x1023;
const uint16_t kWPSElementModelNumber = 0x1024;
const uint16_t kWPSElementDeviceName = 0x1011;

// This structure is incomplete.  Fields will be added as necessary.
//
// NOTE: the uint16_t stuff is in little-endian format so conversions are
// required.
struct ieee80211_frame {
  uint16_t frame_control;
  uint16_t duration_usec;
  uint8_t destination_mac[6];
  uint8_t source_mac[6];
  uint8_t address[6];
  uint16_t sequence_control;
  union {
    struct {
      uint16_t reserved_1;
      uint16_t reserved_2;
      uint16_t status_code;
    } authentiate_message;
    struct {
      uint16_t reason_code;
    } deauthentiate_message;
    struct {
      uint16_t reserved_1;
      uint16_t status_code;
    } associate_response;
  } u;
};

// Status/reason code returned by nl80211 messages: Authenticate,
// Deauthenticate, Associate, and Reassociate.
enum ConnectStatus {
  kConnectStatusSuccessful = 0,
  kConnectStatusFailure = 1,
  kConnectStatusNoLongerValid = 2,
  kConnectStatusSenderHasLeft = 3,
  kConnectStatusNonAuthenticated = 7,
  kConnectStatusAllCapabilitiesNotSupported = 10,
  kConnectStatusCantConfirmAssociation = 11,
  kConnectStatusAssociationDenied = 12,
  kConnectStatusAuthenticationUnsupported = 13,
  kConnectStatusOutOfSequence = 14,
  kConnectStatusChallengeFailure = 15,
  kConnectStatusFrameTimeout = 16,
  kConnectStatusMaxSta = 17,
  kConnectStatusDataRateUnsupported = 18,
  kConnectStatusShortPreambleUnsupported = 19,
  kConnectStatusPbccUnsupported = 20,
  kConnectStatusChannelAgilityUnsupported = 21,
  kConnectStatusNeedSpectrumManagement = 22,
  kConnectStatusUnacceptablePowerCapability = 23,
  kConnectStatusUnacceptableSupportedChannelInfo = 24,
  kConnectStatusShortTimeSlotRequired = 25,
  kConnectStatusErPbccRequired = 26,
  kConnectStatusHtFeaturesRequired = 27,
  kConnectStatusR0khUnreachable = 28,
  kConnectStatusPcoTransitionRequired = 29,
  kConnectStatusRejectedTemporarily = 30,
  kConnectStatusRobustPolicyViolated = 31,
  kConnectStatusQosFailure = 32,
  kConnectStatusInsufficientBandwithForQsta = 33,
  kConnectStatusPoorConditions = 34,
  kConnectStatusQosNotSupported = 35,
  kConnectStatusDeclined = 37,
  kConnectStatusInvalidParameterValues = 38,
  kConnectStatusCannotBeHonored = 39,
  kConnectStatusInvalidInfoElement = 40,
  kConnectStatusGroupCipherInvalid = 41,
  kConnectStatusPairwiseCipherInvalid = 42,
  kConnectStatusAkmpInvalid = 43,
  kConnectStatusUnsupportedRsnIeVersion = 44,
  kConnectStatusInvalidRsnIeCaps = 45,
  kConnectStatusCipherSuiteRejected = 46,
  kConnectStatusTsDelayNotMet = 47,
  kConnectStatusDirectLinkIllegal = 48,
  kConnectStatusStaNotInQbss = 49,
  kConnectStatusStaNotInQsta = 50,
  kConnectStatusExcessiveListenInterval = 51,
  kConnectStatusInvalidFastBssFrameCount = 52,
  kConnectStatusInvalidPmkid = 53,
  kConnectStatusInvalidMdie = 54,
  kConnectStatusInvalidFtie = 55,
};

}

}  // namespace shill

#endif  // SHILL_IEEE_80211_H
