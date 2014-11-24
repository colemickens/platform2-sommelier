// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/device.h"

#include <string>
#include <vector>

#include <base/strings/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <shill/net/ieee80211.h>
#include <shill/net/nl80211_attribute.h>
#include <shill/net/nl80211_message.h>

using std::vector;

namespace apmanager {

namespace {

const char kDeviceName[] = "phy0";
const Device::WiFiInterface kApModeInterface0 = {
    "uap0", kDeviceName, 1, NL80211_IFTYPE_AP
};
const Device::WiFiInterface kApModeInterface1 = {
    "uap1", kDeviceName, 2, NL80211_IFTYPE_AP
};
const Device::WiFiInterface kManagedModeInterface0 = {
    "wlan0", kDeviceName, 3, NL80211_IFTYPE_STATION
};
const Device::WiFiInterface kManagedModeInterface1 = {
    "wlan1", kDeviceName, 4, NL80211_IFTYPE_STATION
};
const Device::WiFiInterface kMonitorModeInterface = {
    "monitor0", kDeviceName, 5, NL80211_IFTYPE_MONITOR
};

}  // namespace

class DeviceTest : public testing::Test {
 public:
  DeviceTest() : device_(new Device(kDeviceName)) {}

  void VerifyInterfaceList(
      const vector<Device::WiFiInterface>& interface_list) {
    EXPECT_EQ(interface_list.size(), device_->interface_list_.size());
    for (size_t i = 0; i < interface_list.size(); i++) {
      EXPECT_TRUE(interface_list[i].Equals(device_->interface_list_[i]));
    }
  }

  void VerifyPreferredApInterface(const std::string& interface_name) {
    EXPECT_EQ(interface_name, device_->GetPreferredApInterface());
  }

  void AddWiphyBandAttribute(shill::AttributeListRefPtr wiphy_bands,
                             const std::string& band_name,
                             int band_id,
                             std::vector<uint32_t> frequency_list,
                             uint16_t ht_cap_mask) {
    // Band attribute.
    shill::AttributeListRefPtr wiphy_band;
    wiphy_bands->CreateNestedAttribute(band_id, band_name.c_str());
    wiphy_bands->GetNestedAttributeList(band_id, &wiphy_band);
    // Frequencies attribute.
    shill::AttributeListRefPtr frequencies;
    wiphy_band->CreateNestedAttribute(NL80211_BAND_ATTR_FREQS,
                                      "NL80211_BAND_ATTR_FREQS");
    wiphy_band->GetNestedAttributeList(NL80211_BAND_ATTR_FREQS,
                                       &frequencies);
    // Frequency attribute.
    for (size_t i = 0; i < frequency_list.size(); i++) {
      shill::AttributeListRefPtr frequency;
      frequencies->CreateNestedAttribute(
          i, base::StringPrintf("Frequency %d", frequency_list[i]).c_str());
      frequencies->GetNestedAttributeList(i, &frequency);
      frequency->CreateU32Attribute(NL80211_FREQUENCY_ATTR_FREQ,
                                    "NL80211_FREQUENCY_ATTR_FREQ");
      frequency->SetU32AttributeValue(NL80211_FREQUENCY_ATTR_FREQ,
                                      frequency_list[i]);
      frequencies->SetNestedAttributeHasAValue(i);
    }
    wiphy_band->SetNestedAttributeHasAValue(NL80211_BAND_ATTR_FREQS);

    // HT Capability attribute.
    wiphy_band->CreateU16Attribute(NL80211_BAND_ATTR_HT_CAPA,
                                   "NL80211_BAND_ATTR_HT_CAPA");
    wiphy_band->SetU16AttributeValue(NL80211_BAND_ATTR_HT_CAPA, ht_cap_mask);

    wiphy_bands->SetNestedAttributeHasAValue(band_id);
  }

  void VerifyFrequencyList(int band_id, std::vector<uint32_t> frequency_list) {
    EXPECT_EQ(frequency_list, device_->band_capability_[band_id].frequencies);
  }

 protected:
  scoped_refptr<Device> device_;
};

TEST_F(DeviceTest, RegisterInterface) {
  vector<Device::WiFiInterface> interface_list;
  interface_list.push_back(kApModeInterface0);
  interface_list.push_back(kManagedModeInterface0);
  interface_list.push_back(kMonitorModeInterface);

  device_->RegisterInterface(kApModeInterface0);
  device_->RegisterInterface(kManagedModeInterface0);
  device_->RegisterInterface(kMonitorModeInterface);

  // Verify result interface list.
  VerifyInterfaceList(interface_list);
}

TEST_F(DeviceTest, DeregisterInterface) {
  vector<Device::WiFiInterface> interface_list;
  interface_list.push_back(kApModeInterface0);
  interface_list.push_back(kManagedModeInterface0);

  // Register all interfaces, then deregister monitor0 and wlan1 interfaces.
  device_->RegisterInterface(kApModeInterface0);
  device_->RegisterInterface(kMonitorModeInterface);
  device_->RegisterInterface(kManagedModeInterface0);
  device_->RegisterInterface(kManagedModeInterface1);
  device_->DeregisterInterface(kMonitorModeInterface);
  device_->DeregisterInterface(kManagedModeInterface1);

  // Verify result interface list.
  VerifyInterfaceList(interface_list);
}

TEST_F(DeviceTest, PreferredAPInterface) {
  // Register a monitor mode interface, no preferred AP mode interface.
  device_->RegisterInterface(kMonitorModeInterface);
  VerifyPreferredApInterface("");

  // Register a managed mode interface, should be set to preferred AP interface.
  device_->RegisterInterface(kManagedModeInterface0);
  VerifyPreferredApInterface(kManagedModeInterface0.iface_name);

  // Register a ap mode interface, should be set to preferred AP interface.
  device_->RegisterInterface(kApModeInterface0);
  VerifyPreferredApInterface(kApModeInterface0.iface_name);

  // Register another ap mode interface "uap1" and managed mode interface
  // "wlan1", preferred AP interface should still be set to the first detected
  // ap mode interface "uap0".
  device_->RegisterInterface(kApModeInterface1);
  device_->RegisterInterface(kManagedModeInterface1);
  VerifyPreferredApInterface(kApModeInterface0.iface_name);

  // Deregister the first ap mode interface, preferred AP interface should be
  // set to the second ap mode interface.
  device_->DeregisterInterface(kApModeInterface0);
  VerifyPreferredApInterface(kApModeInterface1.iface_name);

  // Deregister the second ap mode interface, preferred AP interface should be
  // set the first managed mode interface.
  device_->DeregisterInterface(kApModeInterface1);
  VerifyPreferredApInterface(kManagedModeInterface0.iface_name);

  // Deregister the first managed mode interface, preferred AP interface
  // should be set to the second managed mode interface.
  device_->DeregisterInterface(kManagedModeInterface0);
  VerifyPreferredApInterface(kManagedModeInterface1.iface_name);

  // Deregister the second managed mode interface, preferred AP interface
  // should be set to empty string.
  device_->DeregisterInterface(kManagedModeInterface1);
  VerifyPreferredApInterface("");
}

TEST_F(DeviceTest, ParseHTCapability) {
  shill::NewWiphyMessage message;
  message.attributes()->CreateNestedAttribute(
      NL80211_ATTR_WIPHY_BANDS, "NL80211_ATTR_WIPHY_BANDS");
  shill::AttributeListRefPtr wiphy_bands;
  message.attributes()->GetNestedAttributeList(
      NL80211_ATTR_WIPHY_BANDS, &wiphy_bands);

  // 2.4GHz band capability.
  const uint32_t kBand24GHzFrequencies[] = {
      2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467};
  const uint16_t kBand24GHzHTCapMask = shill::IEEE_80211::kHTCapMaskLdpcCoding |
                                       shill::IEEE_80211::kHTCapMaskGrnFld |
                                       shill::IEEE_80211::kHTCapMaskSgi20;
  std::vector<uint32_t> band_24ghz_freq_list(
      kBand24GHzFrequencies,
      kBand24GHzFrequencies + sizeof(kBand24GHzFrequencies) /
          sizeof(kBand24GHzFrequencies[0]));
  AddWiphyBandAttribute(
      wiphy_bands, "2.4GHz band", 0, band_24ghz_freq_list,
      kBand24GHzHTCapMask);

  // 5GHz band capability.
  const uint32_t kBand5GHzFrequencies[] = {
      5180, 5190, 5200, 5210, 5220, 5230, 5240, 5260, 5280, 5300, 5320};
  const uint16_t kBand5GHzHTCapMask =
      shill::IEEE_80211::kHTCapMaskLdpcCoding |
      shill::IEEE_80211::kHTCapMaskSupWidth2040 |
      shill::IEEE_80211::kHTCapMaskGrnFld |
      shill::IEEE_80211::kHTCapMaskSgi20 |
      shill::IEEE_80211::kHTCapMaskSgi40;
  std::vector<uint32_t> band_5ghz_freq_list(
      kBand5GHzFrequencies,
      kBand5GHzFrequencies + sizeof(kBand5GHzFrequencies) /
          sizeof(kBand5GHzFrequencies[0]));
  AddWiphyBandAttribute(
      wiphy_bands, "5GHz band", 1, band_5ghz_freq_list, kBand5GHzHTCapMask);

  message.attributes()->SetNestedAttributeHasAValue(NL80211_ATTR_WIPHY_BANDS);

  device_->ParseWiphyCapability(message);

  // Verify frequency list for both bands.
  VerifyFrequencyList(0, band_24ghz_freq_list);
  VerifyFrequencyList(1, band_5ghz_freq_list);

  // Verify HT Capablity for 2.4GHz band.
  const char kBand24GHzHTCapability[] = "[LDPC SMPS-STATIC GF SHORT-GI-20]";
  std::string band_24ghz_cap;
  EXPECT_TRUE(device_->GetHTCapability(6, &band_24ghz_cap));
  EXPECT_EQ(kBand24GHzHTCapability, band_24ghz_cap);

  // Verify HT Capablity for 5GHz band.
  const char kBand5GHzHTCapability[] =
      "[LDPC HT40+ SMPS-STATIC GF SHORT-GI-20 SHORT-GI-40]";
  std::string band_5ghz_cap;
  EXPECT_TRUE(device_->GetHTCapability(36, &band_5ghz_cap));
  EXPECT_EQ(kBand5GHzHTCapability, band_5ghz_cap);
}

}  // namespace apmanager
