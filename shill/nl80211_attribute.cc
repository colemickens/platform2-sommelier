// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nl80211_attribute.h"

#include <ctype.h>
#include <linux/nl80211.h>
#include <netlink/attr.h>
#include <netlink/netlink.h>

#include <iomanip>
#include <string>

#include <base/format_macros.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>

#include "shill/logging.h"
#include "shill/scope_logger.h"

using std::string;

using base::StringAppendF;
using base::StringPrintf;

namespace shill {

Nl80211Attribute::Nl80211Attribute(enum nl80211_attrs name,
                                   const char *name_string,
                                   Type type,
                                   const char *type_string)
    : name_(name), name_string_(name_string), type_(type),
      type_string_(type_string) {}

// static
Nl80211Attribute *Nl80211Attribute::NewFromNlAttr(nl80211_attrs name,
                                                  const nlattr *data) {
  scoped_ptr<Nl80211Attribute> attr;
  switch (name) {
    case NL80211_ATTR_DURATION:
      attr.reset(new Nl80211AttributeDuration());
      break;

    // TODO(wdg): Add more attributes.

    default:
      attr.reset(new Nl80211AttributeGeneric(name));
      break;
  }
  attr->InitFromNlAttr(data);
  return attr.release();
}

// Duplicate attribute data, store in map indexed on |name|.
bool Nl80211Attribute::InitFromNlAttr(const nlattr *other) {
  if (!other) {
    LOG(ERROR) << "NULL data";
    return false;
  }

  data_ = ByteString(reinterpret_cast<const unsigned char *>(other),
                     nla_total_size(nla_len(const_cast<nlattr *>(other))));
  return true;
}

// Retrieves a pointer to the raw attribute data but not the header.
bool Nl80211Attribute::GetRawData(ByteString *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }

  *output = data_;
  return true;
}

string Nl80211Attribute::RawToString() const {
  string output = " === RAW: ";

  nlattr *data_nlattr = const_cast<nlattr *>(data());
  uint16_t length = nla_len(data_nlattr);
  StringAppendF(&output, "len=%u", length);

  const uint8_t *const_data = reinterpret_cast<const uint8_t *>(data());

  output.append(" DATA: ");
  for (int i =0 ; i < length; ++i) {
    StringAppendF(&output, "[%d]=%02x ", i, *(const_data)+i);
  }
  output.append(" ==== ");
  return output;
}

// Nl80211U32Attribute::

const char Nl80211U32Attribute::kMyTypeString[] = "uint32_t";
const Nl80211Attribute::Type Nl80211U32Attribute::kType =
    Nl80211Attribute::kTypeU32;

bool Nl80211U32Attribute::InitFromNlAttr(const nlattr *param) {
  if (!param) {
    LOG(ERROR) << "Null |param| parameter";
    return false;
  }

  uint32_t data = nla_get_u32(const_cast<nlattr *>(param));
  SetU32Value(data);
  return Nl80211Attribute::InitFromNlAttr(param);
}

bool Nl80211U32Attribute::GetU32Value(uint32_t *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  *output = value_;
  return true;
}

bool Nl80211U32Attribute::SetU32Value(uint32_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U32Attribute::AsString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  uint32_t value;
  if (!GetU32Value(&value))
    return false;
  *output = StringPrintf("%" PRIu32, value);
  return true;
}

// Nl80211RawAttribute

const char Nl80211RawAttribute::kMyTypeString[] = "<raw>";
const Nl80211Attribute::Type Nl80211RawAttribute::kType =
    Nl80211Attribute::kTypeRaw;

bool Nl80211RawAttribute::InitFromNlAttr(const nlattr *param) {
  if (!param) {
    LOG(ERROR) << "Null |param| parameter";
    return false;
  }

  return Nl80211Attribute::InitFromNlAttr(param);
}

bool Nl80211RawAttribute::GetRawValue(const ByteString **output) const {
  if (!output) {
    LOG(ERROR) << "NULL |output|";
    return false;
  }
  *output = &data_;
  return true;
}

bool Nl80211RawAttribute::AsString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  const uint8_t *raw_data = reinterpret_cast<const uint8_t *>(data());
  int total_bytes = nla_len(data());
  *output = StringPrintf("%d bytes:", total_bytes);
  for (int i = 0; i < total_bytes; ++i) {
    StringAppendF(output, " 0x%02x", raw_data[i]);
  }
  return true;
}

// Specific Attributes.

const nl80211_attrs Nl80211AttributeDuration::kName = NL80211_ATTR_DURATION;
const char Nl80211AttributeDuration::kNameString[] = "NL80211_ATTR_DURATION";

Nl80211AttributeGeneric::Nl80211AttributeGeneric(nl80211_attrs name)
    : Nl80211RawAttribute(name, "unused-string") {
  StringAppendF(&name_string_, "<UNKNOWN ATTRIBUTE %d>", name);
}

const char *Nl80211AttributeGeneric::name_string() const {
  return name_string_.c_str();
}

}  // namespace shill
