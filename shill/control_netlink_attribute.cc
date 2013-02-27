// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/control_netlink_attribute.h"

#include <netlink/attr.h>

#include "shill/logging.h"
#include "shill/scope_logger.h"

namespace shill {

const int ControlAttributeFamilyId::kName = CTRL_ATTR_FAMILY_ID;
const char ControlAttributeFamilyId::kNameString[] = "CTRL_ATTR_FAMILY_ID";

const int ControlAttributeFamilyName::kName = CTRL_ATTR_FAMILY_NAME;
const char ControlAttributeFamilyName::kNameString[] = "CTRL_ATTR_FAMILY_NAME";

const int ControlAttributeVersion::kName = CTRL_ATTR_VERSION;
const char ControlAttributeVersion::kNameString[] = "CTRL_ATTR_VERSION";

const int ControlAttributeHdrSize::kName = CTRL_ATTR_HDRSIZE;
const char ControlAttributeHdrSize::kNameString[] = "CTRL_ATTR_HDRSIZE";

const int ControlAttributeMaxAttr::kName = CTRL_ATTR_MAXATTR;
const char ControlAttributeMaxAttr::kNameString[] = "CTRL_ATTR_MAXATTR";

const int ControlAttributeAttrOps::kName = CTRL_ATTR_OPS;
const char ControlAttributeAttrOps::kNameString[] = "CTRL_ATTR_OPS";

ControlAttributeAttrOps::ControlAttributeAttrOps()
      : NetlinkNestedAttribute(kName, kNameString) {}

bool ControlAttributeAttrOps::InitFromNlAttr(const nlattr *const_data) {
  static const NestedData kOps[CTRL_ATTR_OP_MAX  + 1] = {
    {{NLA_U32, 0, 0}, "CTRL_ATTR_OP_UNSPEC", NULL, 0, false},
    {{NLA_U32, 0, 0}, "CTRL_ATTR_OP_ID", NULL, 0, false},
    {{NLA_U32, 0, 0}, "CTRL_ATTR_OP_FLAGS", NULL, 0, false},
  };
  static const NestedData kOpsList[] = {
    {{NLA_NESTED, 0, 0 }, "FIRST", &kOps[0], arraysize(kOps), true},
  };

  if (!InitNestedFromNlAttr(value_.get(),
                            kOpsList,
                            arraysize(kOpsList),
                            const_data)) {
    LOG(ERROR) << "InitNestedFromNlAttr() failed";
    return false;
  }
  has_a_value_ = true;
  return true;
}

const int ControlAttributeMcastGroups::kName = CTRL_ATTR_MCAST_GROUPS;
const char ControlAttributeMcastGroups::kNameString[] =
    "CTRL_ATTR_MCAST_GROUPS";

ControlAttributeMcastGroups::ControlAttributeMcastGroups()
      : NetlinkNestedAttribute(kName, kNameString) {}

bool ControlAttributeMcastGroups::InitFromNlAttr(const nlattr *const_data) {
  static const NestedData kMulticast[CTRL_ATTR_MCAST_GRP_MAX  + 1] = {
    {{NLA_U32, 0, 0}, "CTRL_ATTR_MCAST_GRP_UNSPEC", NULL, 0, false},
    {{NLA_STRING, 0, 0}, "CTRL_ATTR_MCAST_GRP_NAME", NULL, 0, false},
    {{NLA_U32, 0, 0}, "CTRL_ATTR_MCAST_GRP_ID", NULL, 0, false},
  };
  static const NestedData kMulticastList[] = {
    {{NLA_NESTED, 0, 0}, "FIRST", &kMulticast[0], arraysize(kMulticast),
      true},
  };

  if (!InitNestedFromNlAttr(value_.get(),
                            kMulticastList,
                            arraysize(kMulticastList),
                            const_data)) {
    LOG(ERROR) << "InitNestedFromNlAttr() failed";
    return false;
  }
  has_a_value_ = true;
  return true;
}

}  // namespace shill
