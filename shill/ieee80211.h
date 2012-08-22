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
enum WiFiReasonCode {
  // 0 is reserved.
  kReasonCodeUnspecified = 1,
  kReasonCodePreviousAuthenticationInvalid = 2,
  kReasonCodeSenderHasLeft = 3,
  kReasonCodeInactivity = 4,
  kReasonCodeTooManySTAs = 5,
  kReasonCodeNonAuthenticated = 6,
  kReasonCodeNonAssociated = 7,
  kReasonCodeDisassociatedHasLeft = 8,
  kReasonCodeReassociationNotAuthenticated = 9,
  kReasonCodeUnacceptablePowerCapability = 10,
  kReasonCodeUnacceptableSupportedChannelInfo = 11,
  // 12 is reserved.
  kReasonCodeInvalidInfoElement = 13,
  kReasonCodeMICFailure = 14,
  kReasonCode4WayTimeout = 15,
  kReasonCodeGroupKeyHandshakeTimeout = 16,
  kReasonCodeDifferenIE = 17,
  kReasonCodeGroupCipherInvalid = 18,
  kReasonCodePairwiseCipherInvalid = 19,
  kReasonCodeAkmpInvalid = 20,
  kReasonCodeUnsupportedRsnIeVersion = 21,
  kReasonCodeInvalidRsnIeCaps = 22,
  kReasonCode8021XAuth = 23,
  kReasonCodeCipherSuiteRejected = 24,
  // 25-31 are reserved.
  kReasonCodeUnspecifiedQoS = 32,
  kReasonCodeQoSBandwidth = 33,
  kReasonCodeiPoorConditions = 34,
  kReasonCodeOutsideTxop = 35,
  kReasonCodeStaLeaving = 36,
  kReasonCodeUnacceptableMechanism = 37,
  kReasonCodeSetupRequired = 38,
  kReasonCodeTimeout = 39,
  kReasonCodeCipherSuiteNotSupported = 45,
  kReasonCodeMax,
  kReasonCodeInvalid = UINT16_MAX
};

enum WiFiStatusCode {
  kStatusCodeSuccessful = 0,
  kStatusCodeFailure = 1,
  // 2-9 are reserved.
  kStatusCodeAllCapabilitiesNotSupported = 10,
  kStatusCodeCantConfirmAssociation = 11,
  kStatusCodeAssociationDenied = 12,
  kStatusCodeAuthenticationUnsupported = 13,
  kStatusCodeOutOfSequence = 14,
  kStatusCodeChallengeFailure = 15,
  kStatusCodeFrameTimeout = 16,
  kStatusCodeMaxSta = 17,
  kStatusCodeDataRateUnsupported = 18,
  kStatusCodeShortPreambleUnsupported = 19,
  kStatusCodePbccUnsupported = 20,
  kStatusCodeChannelAgilityUnsupported = 21,
  kStatusCodeNeedSpectrumManagement = 22,
  kStatusCodeUnacceptablePowerCapability = 23,
  kStatusCodeUnacceptableSupportedChannelInfo = 24,
  kStatusCodeShortTimeSlotRequired = 25,
  kStatusCodeDssOfdmRequired = 26,
  // 27-31 are reserved.
  kStatusCodeQosFailure = 32,
  kStatusCodeInsufficientBandwithForQsta = 33,
  kStatusCodePoorConditions = 34,
  kStatusCodeQosNotSupported = 35,
  // 36 is reserved.
  kStatusCodeDeclined = 37,
  kStatusCodeInvalidParameterValues = 38,
  kStatusCodeCannotBeHonored = 39,
  kStatusCodeInvalidInfoElement = 40,
  kStatusCodeGroupCipherInvalid = 41,
  kStatusCodePairwiseCipherInvalid = 42,
  kStatusCodeAkmpInvalid = 43,
  kStatusCodeUnsupportedRsnIeVersion = 44,
  kStatusCodeInvalidRsnIeCaps = 45,
  kStatusCodeCipherSuiteRejected = 46,
  kStatusCodeTsDelayNotMet = 47,
  kStatusCodeDirectLinkIllegal = 48,
  kStatusCodeStaNotInBss = 49,
  kStatusCodeStaNotInQsta = 50,
  kStatusCodeExcessiveListenInterval = 51,
  kStatusCodeMax,
  kStatusCodeInvalid = UINT16_MAX
};

}  // namespace IEEE_80211

}  // namespace shill

#endif  // SHILL_IEEE_80211_H
