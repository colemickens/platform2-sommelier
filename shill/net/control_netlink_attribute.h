// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_CONTROL_NETLINK_ATTRIBUTE_H_
#define SHILL_NET_CONTROL_NETLINK_ATTRIBUTE_H_

#include <base/macros.h>

#include "shill/net/netlink_attribute.h"

namespace shill {

// Control.

class ControlAttributeFamilyId : public NetlinkU16Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeFamilyId() : NetlinkU16Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ControlAttributeFamilyId);
};

class ControlAttributeFamilyName : public NetlinkStringAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeFamilyName() : NetlinkStringAttribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ControlAttributeFamilyName);
};

class ControlAttributeVersion : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeVersion() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ControlAttributeVersion);
};

class ControlAttributeHdrSize : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeHdrSize() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ControlAttributeHdrSize);
};

class ControlAttributeMaxAttr : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeMaxAttr() : NetlinkU32Attribute(kName, kNameString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ControlAttributeMaxAttr);
};

class ControlAttributeAttrOps : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeAttrOps();

 private:
  DISALLOW_COPY_AND_ASSIGN(ControlAttributeAttrOps);
};

class ControlAttributeMcastGroups : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeMcastGroups();

 private:
  DISALLOW_COPY_AND_ASSIGN(ControlAttributeMcastGroups);
};

}  // namespace shill

#endif  // SHILL_NET_CONTROL_NETLINK_ATTRIBUTE_H_
