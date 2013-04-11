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
      : NetlinkNestedAttribute(kName, kNameString) {
  nested_template_.push_back(
      NestedData(NLA_U32, "__NL80211_ATTR_CQM_INVALID", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_ATTR_CQM_RSSI_THOLD", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_ATTR_CQM_RSSI_HYST", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_ATTR_CQM_PKT_LOSS_EVENT", false));
}

const int Nl80211AttributeDisconnectedByAp::kName =
    NL80211_ATTR_DISCONNECTED_BY_AP;
const char Nl80211AttributeDisconnectedByAp::kNameString[] =
    "NL80211_ATTR_DISCONNECTED_BY_AP";

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

const int Nl80211AttributeReasonCode::kName =
    NL80211_ATTR_REASON_CODE;
const char Nl80211AttributeReasonCode::kNameString[] =
    "NL80211_ATTR_REASON_CODE";

const int Nl80211AttributeRegAlpha2::kName = NL80211_ATTR_REG_ALPHA2;
const char Nl80211AttributeRegAlpha2::kNameString[] = "NL80211_ATTR_REG_ALPHA2";

const int Nl80211AttributeRegInitiator::kName =
    NL80211_ATTR_REG_INITIATOR;
const char Nl80211AttributeRegInitiator::kNameString[] =
    "NL80211_ATTR_REG_INITIATOR";

const int Nl80211AttributeRegType::kName = NL80211_ATTR_REG_TYPE;
const char Nl80211AttributeRegType::kNameString[] = "NL80211_ATTR_REG_TYPE";

const int Nl80211AttributeRespIe::kName = NL80211_ATTR_RESP_IE;
const char Nl80211AttributeRespIe::kNameString[] = "NL80211_ATTR_RESP_IE";

const int Nl80211AttributeScanFrequencies::kName =
    NL80211_ATTR_SCAN_FREQUENCIES;
const char Nl80211AttributeScanFrequencies::kNameString[] =
    "NL80211_ATTR_SCAN_FREQUENCIES";

Nl80211AttributeScanFrequencies::Nl80211AttributeScanFrequencies()
      : NetlinkNestedAttribute(kName, kNameString) {
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_SCAN_FREQ", true));
}

const int Nl80211AttributeScanSsids::kName = NL80211_ATTR_SCAN_SSIDS;
const char Nl80211AttributeScanSsids::kNameString[] = "NL80211_ATTR_SCAN_SSIDS";

Nl80211AttributeScanSsids::Nl80211AttributeScanSsids()
      : NetlinkNestedAttribute(kName, kNameString) {
  nested_template_.push_back(
      NestedData(NLA_STRING, "NL80211_SCAN_SSID", true));
}

const int Nl80211AttributeStaInfo::kName = NL80211_ATTR_STA_INFO;
const char Nl80211AttributeStaInfo::kNameString[] = "NL80211_ATTR_STA_INFO";

Nl80211AttributeStaInfo::Nl80211AttributeStaInfo()
      : NetlinkNestedAttribute(kName, kNameString) {

  NestedData tx_rates(NLA_NESTED, "NL80211_STA_INFO_TX_BITRATE", false);
  tx_rates.deeper_nesting.push_back(
      NestedData(NLA_U32, "__NL80211_RATE_INFO_INVALID", false));
  tx_rates.deeper_nesting.push_back(
      NestedData(NLA_U16, "NL80211_RATE_INFO_BITRATE", false));
  tx_rates.deeper_nesting.push_back(
      NestedData(NLA_U8, "NL80211_RATE_INFO_MCS", false));
  tx_rates.deeper_nesting.push_back(
      NestedData(NLA_FLAG, "NL80211_RATE_INFO_40_MHZ_WIDTH", false));
  tx_rates.deeper_nesting.push_back(
      NestedData(NLA_FLAG, "NL80211_RATE_INFO_SHORT_GI", false));

  NestedData rx_rates(NLA_NESTED, "NL80211_STA_INFO_RX_BITRATE", false);
  rx_rates.deeper_nesting = tx_rates.deeper_nesting;

  NestedData bss(NLA_NESTED, "NL80211_STA_INFO_BSS_PARAM", false);
  bss.deeper_nesting.push_back(
      NestedData(NLA_U32, "__NL80211_STA_BSS_PARAM_INVALID", false));
  bss.deeper_nesting.push_back(
      NestedData(NLA_FLAG, "NL80211_STA_BSS_PARAM_CTS_PROT", false));
  bss.deeper_nesting.push_back(
      NestedData(NLA_FLAG, "NL80211_STA_BSS_PARAM_SHORT_PREAMBLE",
                           false));
  bss.deeper_nesting.push_back(
      NestedData(NLA_FLAG, "NL80211_STA_BSS_PARAM_SHORT_SLOT_TIME",
                           false));
  bss.deeper_nesting.push_back(
      NestedData(NLA_U8, "NL80211_STA_BSS_PARAM_DTIM_PERIOD", false));
  bss.deeper_nesting.push_back(
      NestedData(NLA_U16, "NL80211_STA_BSS_PARAM_BEACON_INTERVAL", false));

  nested_template_.push_back(
      NestedData(NLA_U32, "__NL80211_STA_INFO_INVALID", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_INACTIVE_TIME", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_RX_BYTES", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_TX_BYTES", false));
  nested_template_.push_back(
      NestedData(NLA_U16, "NL80211_STA_INFO_LLID", false));
  nested_template_.push_back(
      NestedData(NLA_U16, "NL80211_STA_INFO_PLID", false));
  nested_template_.push_back(
      NestedData(NLA_U8, "NL80211_STA_INFO_PLINK_STATE", false));
  nested_template_.push_back(
      NestedData(NLA_U8, "NL80211_STA_INFO_SIGNAL", false));
  nested_template_.push_back(tx_rates);
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_RX_PACKETS", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_TX_PACKETS", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_TX_RETRIES", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_TX_FAILED", false));
  nested_template_.push_back(
      NestedData(NLA_U8, "NL80211_STA_INFO_SIGNAL_AVG", false));
  nested_template_.push_back(rx_rates);
  nested_template_.push_back(bss);
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_CONNECTED_TIME", false));
  nested_template_.push_back(
      NestedData(NLA_U64, "NL80211_STA_INFO_STA_FLAGS", false));
  nested_template_.push_back(
      NestedData(NLA_U32, "NL80211_STA_INFO_BEACON_LOSS", false));
}

const int Nl80211AttributeStatusCode::kName =
    NL80211_ATTR_STATUS_CODE;
const char Nl80211AttributeStatusCode::kNameString[] =
    "NL80211_ATTR_STATUS_CODE";

const int Nl80211AttributeSupportMeshAuth::kName =
    NL80211_ATTR_SUPPORT_MESH_AUTH;
const char Nl80211AttributeSupportMeshAuth::kNameString[] =
    "NL80211_ATTR_SUPPORT_MESH_AUTH";

const int Nl80211AttributeTimedOut::kName = NL80211_ATTR_TIMED_OUT;
const char Nl80211AttributeTimedOut::kNameString[] = "NL80211_ATTR_TIMED_OUT";

const int Nl80211AttributeWiphyFreq::kName = NL80211_ATTR_WIPHY_FREQ;
const char Nl80211AttributeWiphyFreq::kNameString[] = "NL80211_ATTR_WIPHY_FREQ";

const int Nl80211AttributeWiphy::kName = NL80211_ATTR_WIPHY;
const char Nl80211AttributeWiphy::kNameString[] = "NL80211_ATTR_WIPHY";

const int Nl80211AttributeWiphyName::kName = NL80211_ATTR_WIPHY_NAME;
const char Nl80211AttributeWiphyName::kNameString[] = "NL80211_ATTR_WIPHY_NAME";

}  // namespace shill
