// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NL80211_ATTRIBUTE_H_
#define SHILL_NL80211_ATTRIBUTE_H_

#include <netlink/attr.h>

#include "shill/netlink_attribute.h"
#include "shill/refptr_types.h"

struct nlattr;

namespace shill {

// U8.

class Nl80211AttributeKeyIdx : public NetlinkU8Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeyIdx() : NetlinkU8Attribute(kName, kNameString) {}
};

class Nl80211AttributeRegType : public NetlinkU8Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegType() : NetlinkU8Attribute(kName, kNameString) {}
};

// U16.

class Nl80211AttributeReasonCode : public NetlinkU16Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeReasonCode() : NetlinkU16Attribute(kName, kNameString) {}
};

class Nl80211AttributeStatusCode : public NetlinkU16Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeStatusCode() : NetlinkU16Attribute(kName, kNameString) {}
};

// U32.

class Nl80211AttributeDuration : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeDuration() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeGeneration : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeGeneration() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeIfindex : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeIfindex() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeKeyType : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeyType() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeRegInitiator : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegInitiator() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeWiphy : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphy() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeWiphyFreq : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphyFreq() : NetlinkU32Attribute(kName, kNameString) {}
};

// U64.

class Nl80211AttributeCookie : public NetlinkU64Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeCookie() : NetlinkU64Attribute(kName, kNameString) {}
};

// Flag.

class Nl80211AttributeDisconnectedByAp : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeDisconnectedByAp() :
    NetlinkFlagAttribute(kName, kNameString) {}
};

class Nl80211AttributeSupportMeshAuth : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeSupportMeshAuth() :
    NetlinkFlagAttribute(kName, kNameString) {}
};

class Nl80211AttributeTimedOut : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeTimedOut() : NetlinkFlagAttribute(kName, kNameString) {}
};

// String.

class Nl80211AttributeRegAlpha2 : public NetlinkStringAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegAlpha2() : NetlinkStringAttribute(kName, kNameString) {}
};

class Nl80211AttributeWiphyName : public NetlinkStringAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphyName() : NetlinkStringAttribute(kName, kNameString) {}
};

// Nested.

class Nl80211AttributeCqm : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeCqm();
  virtual bool InitFromNlAttr(const nlattr *data);
};

class Nl80211AttributeScanFrequencies : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  explicit Nl80211AttributeScanFrequencies();
  virtual bool InitFromNlAttr(const nlattr *const_data);
};

class Nl80211AttributeScanSsids : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  explicit Nl80211AttributeScanSsids();
  virtual bool InitFromNlAttr(const nlattr *const_data);
};

class Nl80211AttributeStaInfo : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeStaInfo();
  virtual bool InitFromNlAttr(const nlattr *const_data);
};

// Raw.

class Nl80211AttributeFrame : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeFrame() : NetlinkRawAttribute(kName, kNameString) {}
};

class Nl80211AttributeKeySeq : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeySeq() : NetlinkRawAttribute(kName, kNameString) {}
};

class Nl80211AttributeMac : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeMac() : NetlinkRawAttribute(kName, kNameString) {}
};

class Nl80211AttributeRespIe : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRespIe() : NetlinkRawAttribute(kName, kNameString) {}
};

}  // namespace shill

#endif  // SHILL_NLATTRIBUTE_H_
