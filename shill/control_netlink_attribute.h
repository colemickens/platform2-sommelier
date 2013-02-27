// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONTROL_NETLINK_ATTRIBUTE_H_
#define SHILL_CONTROL_NETLINK_ATTRIBUTE_H_

#include <netlink/attr.h>

#include "shill/netlink_attribute.h"

struct nlattr;

namespace shill {

// Control.

class ControlAttributeFamilyId : public NetlinkU16Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeFamilyId() : NetlinkU16Attribute(kName, kNameString) {}
};

class ControlAttributeFamilyName : public NetlinkStringAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeFamilyName() : NetlinkStringAttribute(kName, kNameString) {}
};

class ControlAttributeVersion : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeVersion() : NetlinkU32Attribute(kName, kNameString) {}
};

class ControlAttributeHdrSize : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeHdrSize() : NetlinkU32Attribute(kName, kNameString) {}
};

class ControlAttributeMaxAttr : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeMaxAttr() : NetlinkU32Attribute(kName, kNameString) {}
};

class ControlAttributeAttrOps : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeAttrOps();
  virtual bool InitFromNlAttr(const nlattr *data);
};

class ControlAttributeMcastGroups : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  ControlAttributeMcastGroups();
  virtual bool InitFromNlAttr(const nlattr *data);
};

}  // namespace shill

#endif  // SHILL_CONTROL_NETLINK_ATTRIBUTE_H_
