// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nl80211_attribute.h"

#include <netlink/attr.h>

#include "shill/logging.h"

namespace shill {

const int Nl80211AttributeCookie::kName = NL80211_ATTR_COOKIE;
const char Nl80211AttributeCookie::kNameString[] = "NL80211_ATTR_COOKIE";

const int Nl80211AttributeCqm::kName = NL80211_ATTR_CQM;
const char Nl80211AttributeCqm::kNameString[] = "NL80211_ATTR_CQM";

Nl80211AttributeCqm::Nl80211AttributeCqm()
      : NetlinkNestedAttribute(kName, kNameString) {}

bool Nl80211AttributeCqm::InitFromNlAttr(const nlattr *const_data) {
  static const NestedData kCqmValidationTemplate[NL80211_ATTR_CQM_MAX + 1] = {
    {{NLA_U32, 0, 0},  "__NL80211_ATTR_CQM_INVALID", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_ATTR_CQM_RSSI_THOLD", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_ATTR_CQM_RSSI_HYST", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT", NULL, 0,
      false},
    {{NLA_U32, 0, 0}, "NL80211_ATTR_CQM_PKT_LOSS_EVENT", NULL, 0, false},
  };

  if (!InitNestedFromNlAttr(value_.get(),
                            kCqmValidationTemplate,
                            arraysize(kCqmValidationTemplate),
                            const_data)) {
    return false;
  }
  has_a_value_ = true;
  return true;
}

const int Nl80211AttributeDisconnectedByAp::kName
    = NL80211_ATTR_DISCONNECTED_BY_AP;
const char Nl80211AttributeDisconnectedByAp::kNameString[]
    = "NL80211_ATTR_DISCONNECTED_BY_AP";

const int Nl80211AttributeDuration::kName = NL80211_ATTR_DURATION;
const char Nl80211AttributeDuration::kNameString[] = "NL80211_ATTR_DURATION";

const int Nl80211AttributeFrame::kName = NL80211_ATTR_FRAME;
const char Nl80211AttributeFrame::kNameString[] = "NL80211_ATTR_FRAME";

const int Nl80211AttributeGeneration::kName = NL80211_ATTR_GENERATION;
const char Nl80211AttributeGeneration::kNameString[]
    = "NL80211_ATTR_GENERATION";

const int Nl80211AttributeIfindex::kName = NL80211_ATTR_IFINDEX;
const char Nl80211AttributeIfindex::kNameString[] = "NL80211_ATTR_IFINDEX";

const int Nl80211AttributeIftype::kName = NL80211_ATTR_IFTYPE;
const char Nl80211AttributeIftype::kNameString[] = "NL80211_ATTR_IFTYPE";

const int Nl80211AttributeKeyIdx::kName = NL80211_ATTR_KEY_IDX;
const char Nl80211AttributeKeyIdx::kNameString[] = "NL80211_ATTR_KEY_IDX";

const int Nl80211AttributeKeySeq::kName = NL80211_ATTR_KEY_SEQ;
const char Nl80211AttributeKeySeq::kNameString[] = "NL80211_ATTR_KEY_SEQ";

const int Nl80211AttributeKeyType::kName = NL80211_ATTR_KEY_TYPE;
const char Nl80211AttributeKeyType::kNameString[] = "NL80211_ATTR_KEY_TYPE";

const int Nl80211AttributeMac::kName = NL80211_ATTR_MAC;
const char Nl80211AttributeMac::kNameString[] = "NL80211_ATTR_MAC";

const int Nl80211AttributeReasonCode::kName
    = NL80211_ATTR_REASON_CODE;
const char Nl80211AttributeReasonCode::kNameString[]
    = "NL80211_ATTR_REASON_CODE";

const int Nl80211AttributeRegAlpha2::kName = NL80211_ATTR_REG_ALPHA2;
const char Nl80211AttributeRegAlpha2::kNameString[] = "NL80211_ATTR_REG_ALPHA2";

const int Nl80211AttributeRegInitiator::kName
    = NL80211_ATTR_REG_INITIATOR;
const char Nl80211AttributeRegInitiator::kNameString[]
    = "NL80211_ATTR_REG_INITIATOR";

const int Nl80211AttributeRegType::kName = NL80211_ATTR_REG_TYPE;
const char Nl80211AttributeRegType::kNameString[] = "NL80211_ATTR_REG_TYPE";

const int Nl80211AttributeRespIe::kName = NL80211_ATTR_RESP_IE;
const char Nl80211AttributeRespIe::kNameString[] = "NL80211_ATTR_RESP_IE";

const int Nl80211AttributeScanFrequencies::kName
    = NL80211_ATTR_SCAN_FREQUENCIES;
const char Nl80211AttributeScanFrequencies::kNameString[]
    = "NL80211_ATTR_SCAN_FREQUENCIES";

Nl80211AttributeScanFrequencies::Nl80211AttributeScanFrequencies()
      : NetlinkNestedAttribute(kName, kNameString) {}

bool Nl80211AttributeScanFrequencies::InitFromNlAttr(const nlattr *const_data) {
  static const NestedData kScanFrequencyTemplate[] = {
    {{ NLA_U32, 0, 0 }, "NL80211_SCAN_FREQ", NULL, 0, true},
  };

  if (!InitNestedFromNlAttr(value_.get(),
                            kScanFrequencyTemplate,
                            arraysize(kScanFrequencyTemplate),
                            const_data)) {
    LOG(ERROR) << "InitNestedFromNlAttr() failed";
    return false;
  }
  has_a_value_ = true;
  return true;
}

const int Nl80211AttributeScanSsids::kName = NL80211_ATTR_SCAN_SSIDS;
const char Nl80211AttributeScanSsids::kNameString[] = "NL80211_ATTR_SCAN_SSIDS";

Nl80211AttributeScanSsids::Nl80211AttributeScanSsids()
      : NetlinkNestedAttribute(kName, kNameString) {}

bool Nl80211AttributeScanSsids::InitFromNlAttr(const nlattr *const_data) {
  static const NestedData kScanSsidTemplate[] = {
    {{ NLA_STRING, 0, 0 }, "NL80211_SCAN_SSID", NULL, 0, true},
  };

  if (!InitNestedFromNlAttr(value_.get(),
                            kScanSsidTemplate,
                            arraysize(kScanSsidTemplate),
                            const_data)) {
    LOG(ERROR) << "InitNestedFromNlAttr() failed";
    return false;
  }
  has_a_value_ = true;
  return true;
}

const int Nl80211AttributeStaInfo::kName = NL80211_ATTR_STA_INFO;
const char Nl80211AttributeStaInfo::kNameString[] = "NL80211_ATTR_STA_INFO";

Nl80211AttributeStaInfo::Nl80211AttributeStaInfo()
      : NetlinkNestedAttribute(kName, kNameString) {}

bool Nl80211AttributeStaInfo::InitFromNlAttr(const nlattr *const_data) {
  static const NestedData kRateTemplate[NL80211_RATE_INFO_MAX + 1] = {
    {{NLA_U32, 0, 0}, "__NL80211_RATE_INFO_INVALID", NULL, 0, false},
    {{NLA_U16, 0, 0}, "NL80211_RATE_INFO_BITRATE", NULL, 0, false},
    {{NLA_U8, 0, 0}, "NL80211_RATE_INFO_MCS", NULL, 0, false},
    {{NLA_FLAG, 0, 0}, "NL80211_RATE_INFO_40_MHZ_WIDTH", NULL, 0, false},
    {{NLA_FLAG, 0, 0}, "NL80211_RATE_INFO_SHORT_GI", NULL, 0, false},
  };

  static const NestedData kBssTemplate[NL80211_STA_BSS_PARAM_MAX + 1] = {
    {{NLA_U32, 0, 0}, "__NL80211_STA_BSS_PARAM_INVALID", NULL, 0, false},
    {{NLA_FLAG, 0, 0}, "NL80211_STA_BSS_PARAM_CTS_PROT", NULL, 0, false},
    {{NLA_FLAG, 0, 0}, "NL80211_STA_BSS_PARAM_SHORT_PREAMBLE", NULL, 0,
      false},
    {{NLA_FLAG, 0, 0}, "NL80211_STA_BSS_PARAM_SHORT_SLOT_TIME", NULL, 0,
      false},
    {{NLA_U8, 0, 0}, "NL80211_STA_BSS_PARAM_DTIM_PERIOD", NULL, 0, false},
    {{NLA_U16, 0, 0}, "NL80211_STA_BSS_PARAM_BEACON_INTERVAL", NULL, 0,
      false},
  };

  static const NestedData kStationInfoTemplate[NL80211_STA_INFO_MAX + 1] = {
    {{NLA_U32, 0, 0}, "__NL80211_STA_INFO_INVALID", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_INACTIVE_TIME", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_RX_BYTES", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_TX_BYTES", NULL, 0, false},
    {{NLA_U16, 0, 0}, "NL80211_STA_INFO_LLID", NULL, 0, false},
    {{NLA_U16, 0, 0}, "NL80211_STA_INFO_PLID", NULL, false},
    {{NLA_U8, 0, 0}, "NL80211_STA_INFO_PLINK_STATE", NULL, 0, false},
    {{NLA_U8, 0, 0}, "NL80211_STA_INFO_SIGNAL", NULL, 0, false},
    {{NLA_NESTED, 0, 0}, "NL80211_STA_INFO_TX_BITRATE", &kRateTemplate[0],
      arraysize(kRateTemplate), false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_RX_PACKETS", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_TX_PACKETS", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_TX_RETRIES", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_TX_FAILED", NULL, 0, false},
    {{NLA_U8, 0, 0}, "NL80211_STA_INFO_SIGNAL_AVG", NULL, 0, false},
    {{NLA_NESTED, 0, 0}, "NL80211_STA_INFO_RX_BITRATE", &kRateTemplate[0],
      arraysize(kRateTemplate), false},
    {{NLA_NESTED, 0, 0}, "NL80211_STA_INFO_BSS_PARAM", &kBssTemplate[0],
      arraysize(kBssTemplate), false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_CONNECTED_TIME", NULL, 0, false},
    {{NLA_U64, 0, 0}, "NL80211_STA_INFO_STA_FLAGS", NULL, 0, false},
    {{NLA_U32, 0, 0}, "NL80211_STA_INFO_BEACON_LOSS", NULL, 0, false},
  };

  if (!InitNestedFromNlAttr(value_.get(),
                            kStationInfoTemplate,
                            arraysize(kStationInfoTemplate),
                            const_data)) {
    LOG(ERROR) << "InitNestedFromNlAttr() failed";
    return false;
  }
  has_a_value_ = true;
  return true;
}

const int Nl80211AttributeStatusCode::kName
    = NL80211_ATTR_STATUS_CODE;
const char Nl80211AttributeStatusCode::kNameString[]
    = "NL80211_ATTR_STATUS_CODE";

const int Nl80211AttributeSupportMeshAuth::kName
    = NL80211_ATTR_SUPPORT_MESH_AUTH;
const char Nl80211AttributeSupportMeshAuth::kNameString[]
    = "NL80211_ATTR_SUPPORT_MESH_AUTH";

const int Nl80211AttributeTimedOut::kName = NL80211_ATTR_TIMED_OUT;
const char Nl80211AttributeTimedOut::kNameString[] = "NL80211_ATTR_TIMED_OUT";

const int Nl80211AttributeWiphyFreq::kName = NL80211_ATTR_WIPHY_FREQ;
const char Nl80211AttributeWiphyFreq::kNameString[] = "NL80211_ATTR_WIPHY_FREQ";

const int Nl80211AttributeWiphy::kName = NL80211_ATTR_WIPHY;
const char Nl80211AttributeWiphy::kNameString[] = "NL80211_ATTR_WIPHY";

const int Nl80211AttributeWiphyName::kName = NL80211_ATTR_WIPHY_NAME;
const char Nl80211AttributeWiphyName::kNameString[] = "NL80211_ATTR_WIPHY_NAME";

}  // namespace shill
