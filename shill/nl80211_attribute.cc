// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nl80211_attribute.h"

#include <netlink/attr.h>

#include <string>

#include <base/bind.h>
#include <base/format_macros.h>
#include <base/stringprintf.h>

#include "shill/logging.h"

using base::Bind;
using base::StringPrintf;
using std::string;

namespace shill {

const int Nl80211AttributeCookie::kName = NL80211_ATTR_COOKIE;
const char Nl80211AttributeCookie::kNameString[] = "NL80211_ATTR_COOKIE";

const int Nl80211AttributeBss::kName = NL80211_ATTR_BSS;
const char Nl80211AttributeBss::kNameString[] = "NL80211_ATTR_BSS";
const int Nl80211AttributeBss::kChannelsAttributeId = 0x24;
const int Nl80211AttributeBss::kChallengeTextAttributeId = 0x10;
const int Nl80211AttributeBss::kCountryInfoAttributeId = 0x07;
const int Nl80211AttributeBss::kDSParameterSetAttributeId = 0x03;
const int Nl80211AttributeBss::kErpAttributeId = 0x2a;
const int Nl80211AttributeBss::kExtendedRatesAttributeId = 0x32;
const int Nl80211AttributeBss::kHtCapAttributeId = 0x2d;
const int Nl80211AttributeBss::kHtInfoAttributeId = 0x3d;
const int Nl80211AttributeBss::kPowerCapabilityAttributeId = 0x21;
const int Nl80211AttributeBss::kPowerConstraintAttributeId = 0x20;
const int Nl80211AttributeBss::kRequestAttributeId = 0x0a;
const int Nl80211AttributeBss::kRsnAttributeId = 0x30;
const int Nl80211AttributeBss::kSsidAttributeId = 0x00;
const int Nl80211AttributeBss::kSupportedRatesAttributeId = 0x01;
const int Nl80211AttributeBss::kTcpReportAttributeId = 0x23;
const int Nl80211AttributeBss::kVendorSpecificAttributeId = 0xdd;

static const char kSsidString[] = "SSID";
static const char kRatesString[] = "Rates";

Nl80211AttributeBss::Nl80211AttributeBss()
      : NetlinkNestedAttribute(kName, kNameString) {
  nested_template_.push_back(NestedData(NLA_U32, "__NL80211_BSS_INVALID",
                                        false));
  nested_template_.push_back(NestedData(NLA_UNSPEC, "NL80211_BSS_BSSID",
                                        false));
  nested_template_.push_back(NestedData(NLA_U32, "NL80211_BSS_FREQUENCY",
                                        false));
  nested_template_.push_back(NestedData(NLA_U64, "NL80211_BSS_TSF", false));
  nested_template_.push_back(NestedData(NLA_U16, "NL80211_BSS_BEACON_INTERVAL",
                                        false));
  nested_template_.push_back(NestedData(NLA_U16, "NL80211_BSS_CAPABILITY",
                                        false));
  nested_template_.push_back(NestedData(
      NLA_UNSPEC, "NL80211_BSS_INFORMATION_ELEMENTS", false,
      Bind(&Nl80211AttributeBss::ParseInformationElements)));
  nested_template_.push_back(NestedData(NLA_U32, "NL80211_BSS_SIGNAL_MBM",
                                        false));
  nested_template_.push_back(NestedData(NLA_U8, "NL80211_BSS_SIGNAL_UNSPEC",
                                        false));
  nested_template_.push_back(NestedData(NLA_U32, "NL80211_BSS_STATUS",
                                        false));
  nested_template_.push_back(NestedData(NLA_U32, "NL80211_BSS_SEEN_MS_AGO",
                                        false));
  nested_template_.push_back(NestedData(NLA_UNSPEC, "NL80211_BSS_BEACON_IES",
                                        false));
}

bool Nl80211AttributeBss::ParseInformationElements(
    AttributeList *attribute_list, size_t id, const string &attribute_name,
    ByteString data) {
  if (!attribute_list) {
    LOG(ERROR) << "NULL |attribute_list| parameter";
    return false;
  }
  attribute_list->CreateNestedAttribute(id, attribute_name.c_str());

  // Now, handle the nested data.
  AttributeListRefPtr ie_attribute;
  if (!attribute_list->GetNestedAttributeList(id, &ie_attribute) ||
      !ie_attribute) {
    LOG(ERROR) << "Couldn't get attribute " << attribute_name
               << " which we just created.";
    return false;
  }
  while (data.GetLength()) {
    const uint8_t *sub_attribute = data.GetConstData();
    const size_t kHeaderBytes = 2;
    uint8_t type = sub_attribute[0];
    uint8_t payload_bytes = sub_attribute[1];
    const uint8_t *payload = &sub_attribute[kHeaderBytes];
    // See http://dox.ipxe.org/ieee80211_8h_source.html for more info on types
    // and data inside information elements.
    switch (type) {
      case kSsidAttributeId: {
        ie_attribute->CreateSsidAttribute(type, kSsidString);
        if (payload_bytes == 0) {
          ie_attribute->SetStringAttributeValue(type, "");
        } else {
          ie_attribute->SetStringAttributeValue(type, string(
              reinterpret_cast<const char *>(payload), payload_bytes));
        }
        break;
      }
      case kSupportedRatesAttributeId:
      case kExtendedRatesAttributeId: {
        ie_attribute->CreateNestedAttribute(type, kRatesString);
        AttributeListRefPtr rates_attribute;
        if (!ie_attribute->GetNestedAttributeList(type, &rates_attribute) ||
            !rates_attribute) {
          LOG(ERROR) << "Couldn't get attribute " << attribute_name
                     << " which we just created.";
          break;
        }
        // Extract each rate, add it to the list.
        for (size_t i = 0; i < payload_bytes; ++i) {
          string rate_name = StringPrintf("Rate-%zu", i);
          rates_attribute->CreateU8Attribute(i, rate_name.c_str());
          rates_attribute->SetU8AttributeValue(i, payload[i]);
        }
        ie_attribute->SetNestedAttributeHasAValue(type);
        break;
      }
      case kDSParameterSetAttributeId:
      case kCountryInfoAttributeId:
      case kRequestAttributeId:
      case kChallengeTextAttributeId:
      case kPowerConstraintAttributeId:
      case kPowerCapabilityAttributeId:
      case kTcpReportAttributeId:
      case kChannelsAttributeId:
      case kErpAttributeId:
      case kHtCapAttributeId:
      case kRsnAttributeId:
      case kHtInfoAttributeId:
      case kVendorSpecificAttributeId:
      default:
        break;
    }
    data.RemovePrefix(kHeaderBytes + payload_bytes);
  }
  attribute_list->SetNestedAttributeHasAValue(id);
  return true;
}

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
const char Nl80211AttributeGeneration::kNameString[] =
    "NL80211_ATTR_GENERATION";

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
