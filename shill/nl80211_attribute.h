// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NL80211_ATTRIBUTE_H_
#define SHILL_NL80211_ATTRIBUTE_H_

#include <netlink/attr.h>

#include <base/basictypes.h>

#include "shill/netlink_attribute.h"

struct nlattr;

namespace shill {

// U8.

class Nl80211AttributeKeyIdx : public NetlinkU8Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeyIdx() : NetlinkU8Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeKeyIdx);
};

class Nl80211AttributeRegType : public NetlinkU8Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegType() : NetlinkU8Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeRegType);
};

// U16.

class Nl80211AttributeReasonCode : public NetlinkU16Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeReasonCode() : NetlinkU16Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeReasonCode);
};

class Nl80211AttributeStatusCode : public NetlinkU16Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeStatusCode() : NetlinkU16Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeStatusCode);
};

// U32.

class Nl80211AttributeDuration : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeDuration() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeDuration);
};

class Nl80211AttributeGeneration : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeGeneration() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeGeneration);
};

class Nl80211AttributeIfindex : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeIfindex() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeIfindex);
};

class Nl80211AttributeIftype : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeIftype() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeIftype);
};

class Nl80211AttributeKeyType : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeyType() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeKeyType);
};

class Nl80211AttributeRegInitiator : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegInitiator() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeRegInitiator);
};

class Nl80211AttributeWiphy : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphy() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeWiphy);
};

class Nl80211AttributeWiphyFreq : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphyFreq() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeWiphyFreq);
};

// U64.

class Nl80211AttributeCookie : public NetlinkU64Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeCookie() : NetlinkU64Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeCookie);
};

// Flag.

class Nl80211AttributeDisconnectedByAp : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeDisconnectedByAp() :
    NetlinkFlagAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeDisconnectedByAp);
};

class Nl80211AttributeSupportMeshAuth : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeSupportMeshAuth() :
    NetlinkFlagAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeSupportMeshAuth);
};

class Nl80211AttributeTimedOut : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeTimedOut() : NetlinkFlagAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeTimedOut);
};

// String.

class Nl80211AttributeRegAlpha2 : public NetlinkStringAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegAlpha2() : NetlinkStringAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeRegAlpha2);
};

class Nl80211AttributeWiphyName : public NetlinkStringAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphyName() : NetlinkStringAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeWiphyName);
};

// Nested.

class Nl80211AttributeBss : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  // These are sorted alphabetically.
  static const int kChallengeTextAttributeId;
  static const int kChannelsAttributeId;
  static const int kCountryInfoAttributeId;
  static const int kDSParameterSetAttributeId;
  static const int kErpAttributeId;
  static const int kExtendedRatesAttributeId;
  static const int kHtCapAttributeId;
  static const int kHtInfoAttributeId;
  static const int kPowerCapabilityAttributeId;
  static const int kPowerConstraintAttributeId;
  static const int kRequestAttributeId;
  static const int kRsnAttributeId;
  static const int kSsidAttributeId;
  static const int kSupportedRatesAttributeId;
  static const int kTcpReportAttributeId;
  static const int kVendorSpecificAttributeId;

  Nl80211AttributeBss();

 private:
  static bool ParseInformationElements(AttributeList *attribute_list,
                                       size_t id,
                                       const std::string &attribute_name,
                                       ByteString data);

  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeBss);
};

class Nl80211AttributeCqm : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeCqm();

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeCqm);
};

class Nl80211AttributeScanFrequencies : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  explicit Nl80211AttributeScanFrequencies();

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeScanFrequencies);
};

class Nl80211AttributeScanSsids : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  explicit Nl80211AttributeScanSsids();

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeScanSsids);
};

class Nl80211AttributeStaInfo : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeStaInfo();

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeStaInfo);
};

// Raw.

class Nl80211AttributeFrame : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeFrame() : NetlinkRawAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeFrame);
};

class Nl80211AttributeKeySeq : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeySeq() : NetlinkRawAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeKeySeq);
};

class Nl80211AttributeMac : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeMac() : NetlinkRawAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeMac);
};

class Nl80211AttributeRespIe : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRespIe() : NetlinkRawAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211AttributeRespIe);
};

}  // namespace shill

#endif  // SHILL_NL80211_ATTRIBUTE_H_
