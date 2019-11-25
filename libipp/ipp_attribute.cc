// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libipp/ipp_attribute.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <string>
#include <vector>

namespace {

ipp::InternalType InternalTypeForUnknownAttribute(ipp::AttrType type) {
  switch (type) {
    case ipp::AttrType::collection:
      return ipp::InternalType::kCollection;
    case ipp::AttrType::boolean:
    case ipp::AttrType::integer:
    case ipp::AttrType::enum_:
      return ipp::InternalType::kInteger;
    case ipp::AttrType::dateTime:
      return ipp::InternalType::kDateTime;
    case ipp::AttrType::resolution:
      return ipp::InternalType::kResolution;
    case ipp::AttrType::rangeOfInteger:
      return ipp::InternalType::kRangeOfInteger;
    case ipp::AttrType::name:
    case ipp::AttrType::text:
      return ipp::InternalType::kStringWithLanguage;
    default:
      return ipp::InternalType::kString;
  }
}

std::string UnsignedToString(size_t x) {
  std::string s;
  do {
    s.push_back('0' + (x % 10));
    x /= 10;
  } while (x > 0);
  std::reverse(s.begin(), s.end());
  return s;
}
}  // namespace

namespace ipp {

namespace {

// This struct exposes single static method performing conversion between values
// of different types. Returns true if conversion succeeded and false otherwise.
// |out_val| cannot be nullptr.
template <typename InputType, typename OutputType>
struct Converter {
  static bool Convert(AttrName name,
                      const AttrDef& def,
                      const InputType& in_val,
                      OutputType* out_val) {
    return false;
  }
};
template <typename Type>
struct Converter<Type, Type> {
  static bool Convert(AttrName name,
                      const AttrDef& def,
                      const Type& in_val,
                      Type* out_val) {
    *out_val = in_val;
    return true;
  }
};
template <>
struct Converter<std::string, std::string> {
  static bool Convert(AttrName name,
                      const AttrDef& def,
                      const std::string& in_val,
                      std::string* out_val) {
    *out_val = in_val;
    return true;
  }
};
template <typename InputType>
struct Converter<InputType, std::string> {
  static bool Convert(AttrName name,
                      const AttrDef& def,
                      const InputType& in_val,
                      std::string* out_val) {
    *out_val = ToString(in_val);
    return true;
  }
};
template <>
struct Converter<int32_t, std::string> {
  static bool Convert(AttrName name,
                      const AttrDef& def,
                      int32_t in_val,
                      std::string* out_val) {
    if (def.ipp_type == AttrType::boolean) {
      *out_val = ToString(static_cast<bool>(in_val));
    } else if (def.ipp_type == AttrType::enum_ ||
               def.ipp_type == AttrType::keyword) {
      *out_val = ToString(name, in_val);
    } else if (def.ipp_type == AttrType::integer) {
      *out_val = ToString(in_val);
    } else {
      return false;
    }
    return true;
  }
};
template <>
struct Converter<std::string, bool> {
  static bool Convert(AttrName name,
                      const AttrDef& def,
                      const std::string& in_val,
                      bool* out_val) {
    return FromString(in_val, out_val);
  }
};
template <>
struct Converter<std::string, int32_t> {
  static bool Convert(AttrName name,
                      const AttrDef& def,
                      const std::string& in_val,
                      int32_t* out_val) {
    bool result = false;
    if (def.ipp_type == AttrType::boolean) {
      bool out;
      result = FromString(in_val, &out);
      if (result)
        *out_val = out;
    } else if (def.ipp_type == AttrType::enum_ ||
               def.ipp_type == AttrType::keyword) {
      int out;
      result = FromString(in_val, name, &out);
      if (result)
        *out_val = out;
    } else if (def.ipp_type == AttrType::integer) {
      int out;
      result = FromString(in_val, &out);
      if (result)
        *out_val = out;
    }
    return result;
  }
};
template <>
struct Converter<std::string, StringWithLanguage> {
  static bool Convert(AttrName name,
                      const AttrDef& def,
                      const std::string& in_val,
                      StringWithLanguage* out_val) {
    out_val->language = "";
    out_val->value = in_val;
    return true;
  }
};

// Creates new value for attribute |def| and saves it as void*.
template <typename Type>
void* CreateValue(const AttrDef& def) {
  if (sizeof(Type) <= sizeof(void*) && alignof(Type) <= alignof(void*))
    return 0;
  return new Type();
}
template <>
void* CreateValue<Collection*>(const AttrDef& def) {
  return def.constructor();
}

// Deletes value saved as void*.
template <typename Type>
void DeleteValue(void* value) {
  if (sizeof(Type) <= sizeof(void*) && alignof(Type) <= alignof(void*))
    return;
  delete reinterpret_cast<Type*>(value);
}
template <>
void DeleteValue<Collection*>(void* value) {
  delete reinterpret_cast<Collection*>(value);
}

// Returns pointer to a value stored as void*.
template <typename Type>
Type* ReadValuePtr(void** value) {
  if (sizeof(Type) <= sizeof(void*) && alignof(Type) <= alignof(void*))
    return reinterpret_cast<Type*>(value);
  return reinterpret_cast<Type*>(*value);
}

// Const version of the template function above.
template <typename Type>
const Type* ReadValueConstPtr(void* const* value) {
  if (sizeof(Type) <= sizeof(void*) && alignof(Type) <= alignof(void*))
    return reinterpret_cast<const Type*>(value);
  return reinterpret_cast<Type* const>(*value);
}

// Resizes vector of values in an attribute |def|.
template <typename Type>
void ResizeVector(const AttrDef& def, std::vector<Type>* v, size_t new_size) {
  v->resize(new_size);
}
template <>
void ResizeVector<Collection*>(const AttrDef& def,
                               std::vector<Collection*>* v,
                               size_t new_size) {
  const size_t old_size = v->size();
  for (size_t i = new_size; i < old_size; ++i)
    delete v->at(i);
  v->resize(new_size);
  for (size_t i = old_size; i < new_size; ++i)
    (*v)[i] = def.constructor();
}

// Deletes whole attribute |name|,|def| from |values|.
template <typename Type>
void DeleteAttrTyped(std::map<AttrName, void*>* values,
                     AttrName name,
                     const AttrDef& def) {
  auto it = values->find(name);
  if (it == values->end())
    return;
  if (def.is_a_set) {
    auto pv = reinterpret_cast<std::vector<Type>*>(it->second);
    ResizeVector<Type>(def, pv, 0);
    delete pv;
  } else {
    DeleteValue<Type>(it->second);
  }
  values->erase(it);
}

// The same as previous one, just chooses correct template instantiation.
void DeleteAttr(std::map<AttrName, void*>* values,
                AttrName name,
                const AttrDef& def) {
  switch (def.cc_type) {
    case InternalType::kInteger:
      DeleteAttrTyped<int32_t>(values, name, def);
      break;
    case InternalType::kString:
      DeleteAttrTyped<std::string>(values, name, def);
      break;
    case InternalType::kResolution:
      DeleteAttrTyped<Resolution>(values, name, def);
      break;
    case InternalType::kRangeOfInteger:
      DeleteAttrTyped<RangeOfInteger>(values, name, def);
      break;
    case InternalType::kDateTime:
      DeleteAttrTyped<DateTime>(values, name, def);
      break;
    case InternalType::kStringWithLanguage:
      DeleteAttrTyped<StringWithLanguage>(values, name, def);
      break;
    case InternalType::kCollection:
      DeleteAttrTyped<Collection*>(values, name, def);
      break;
  }
}

// Returns a pointer to a value at position |index| in an attribute |name|.
// If the attribute is too short, it is resized to (|index|+1) when possible.
// When |cut_if_longer| is set, the attribute is shrunk to (|index|+1) values if
// it is longer. If |cut_if_longer| equals false, the attribute is no
// downsized. Returns nullptr <=> the attribute is too short and cannot be
// resized to reach (|index|+1) values.
template <typename Type>
Type* ResizeAttrGetValuePtr(std::map<AttrName, void*>* values,
                            AttrName name,
                            const AttrDef& def,
                            size_t index,
                            bool cut_if_longer) {
  if (!def.is_a_set && index > 0)
    return nullptr;
  // Get an entry from |values|, add new if not exists.
  auto it_inserted = values->insert({name, nullptr});
  std::map<AttrName, void*>::iterator it = it_inserted.first;
  if (it_inserted.second) {
    if (def.is_a_set) {
      it->second = new std::vector<Type>();
    } else {
      it->second = CreateValue<Type>(def);
    }
  }
  // Returns the pointer, resize the attribute when needed.
  if (def.is_a_set) {
    std::vector<Type>* v = reinterpret_cast<std::vector<Type>*>(it->second);
    if (cut_if_longer || v->size() <= index)
      ResizeVector<Type>(def, v, index + 1);
    return (v->data() + index);
  } else {
    return ReadValuePtr<Type>(&it->second);
  }
}

// Resizes an attribute |name|,|def| to |new_size| values. The parameter
// |cut_if_longer| works in the same way as in the previous template function.
// Returns false when the attribute is not a set and |new_size| > 1.
bool ResizeAttr(std::map<AttrName, void*>* values,
                AttrName name,
                const AttrDef& def,
                size_t new_size,
                bool cut_if_longer) {
  if (new_size == 0) {
    DeleteAttr(values, name, def);
    return true;
  }
  switch (def.cc_type) {
    case InternalType::kInteger:
      return ResizeAttrGetValuePtr<int32_t>(values, name, def, new_size - 1,
                                            cut_if_longer) != nullptr;
    case InternalType::kString:
      return ResizeAttrGetValuePtr<std::string>(values, name, def, new_size - 1,
                                                cut_if_longer) != nullptr;
    case InternalType::kResolution:
      return ResizeAttrGetValuePtr<Resolution>(values, name, def, new_size - 1,
                                               cut_if_longer) != nullptr;
    case InternalType::kRangeOfInteger:
      return ResizeAttrGetValuePtr<RangeOfInteger>(
                 values, name, def, new_size - 1, cut_if_longer) != nullptr;
    case InternalType::kDateTime:
      return ResizeAttrGetValuePtr<DateTime>(values, name, def, new_size - 1,
                                             cut_if_longer) != nullptr;
    case InternalType::kStringWithLanguage:
      return ResizeAttrGetValuePtr<StringWithLanguage>(
                 values, name, def, new_size - 1, cut_if_longer) != nullptr;
    case InternalType::kCollection:
      return ResizeAttrGetValuePtr<Collection*>(values, name, def, new_size - 1,
                                                cut_if_longer) != nullptr;
  }
  return false;
}

// Reads a value at position |index| in an attribute |name|,|def| and saves it
// to |value|. Proper conversion is applied when needed. The function returns
// true if succeeds and false when one of the following occurs:
// * |value| is nullptr
// * the attribute has less than |index|+1 values
// * required conversion is not possible
template <typename InternalType, typename ApiType>
bool ReadConvertValueTyped(const std::map<AttrName, void*>& values,
                           AttrName name,
                           const AttrDef& def,
                           size_t index,
                           ApiType* value) {
  if (value == nullptr)
    return false;
  auto it = values.find(name);
  if (it == values.end())
    return false;
  const InternalType* internal_value = nullptr;
  if (def.is_a_set) {
    auto v = ReadValueConstPtr<std::vector<InternalType>>(&it->second);
    if (v->size() <= index)
      return false;
    internal_value = v->data() + index;
  } else {
    if (index != 0)
      return false;
    internal_value = ReadValueConstPtr<InternalType>(&it->second);
  }
  return Converter<InternalType, ApiType>::Convert(name, def, *internal_value,
                                                   value);
}

// The same as previous one, just chooses correct template instantiation.
template <typename ApiType>
bool ReadConvertValue(const std::map<AttrName, void*>& values,
                      AttrName name,
                      const AttrDef& def,
                      size_t index,
                      ApiType* value) {
  switch (def.cc_type) {
    case InternalType::kInteger:
      return ReadConvertValueTyped<int32_t, ApiType>(values, name, def, index,
                                                     value);
    case InternalType::kString:
      return ReadConvertValueTyped<std::string, ApiType>(values, name, def,
                                                         index, value);
    case InternalType::kResolution:
      return ReadConvertValueTyped<Resolution, ApiType>(values, name, def,
                                                        index, value);
    case InternalType::kRangeOfInteger:
      return ReadConvertValueTyped<RangeOfInteger, ApiType>(values, name, def,
                                                            index, value);
    case InternalType::kDateTime:
      return ReadConvertValueTyped<DateTime, ApiType>(values, name, def, index,
                                                      value);
    case InternalType::kStringWithLanguage:
      return ReadConvertValueTyped<StringWithLanguage, ApiType>(
          values, name, def, index, value);
    case InternalType::kCollection:
      return false;
  }
  return false;
}

// Saves |value| to position |index| in an attribute |name|,|def|. Proper
// conversion is applied when needed. The attribute is also resized when |index|
// is greater than the attribute's size. The function returns true if succeeds
// and false when one of the following occurs:
// * the attribute is not a set and |index| > 0
// * required conversion is not possible (|value| is incorrect)
template <typename InternalType, typename ApiType>
bool SaveValueTyped(std::map<AttrName, void*>* values,
                    AttrName name,
                    const AttrDef& def,
                    size_t index,
                    const ApiType& value) {
  InternalType internal_value;
  if (!Converter<ApiType, InternalType>::Convert(name, def, value,
                                                 &internal_value))
    return false;
  InternalType* internal_ptr =
      ResizeAttrGetValuePtr<InternalType>(values, name, def, index, false);
  if (internal_ptr == nullptr)
    return false;
  *internal_ptr = internal_value;
  return true;
}

}  // end of namespace

std::string ToString(AttrState s) {
  switch (s) {
    case AttrState::unset:
      return "unset";
    case AttrState::set:
      return "set";
    case AttrState::unsupported:
      return "unsupported";
    case AttrState::unknown:
      return "unknown";
    case AttrState::novalue_:
      return "novalue";
    case AttrState::not_settable:
      return "not-settable";
    case AttrState::delete_attribute:
      return "delete-attribute";
    case AttrState::admin_define:
      return "admin-define";
  }
  return "";
}

std::string ToString(AttrType at) {
  switch (at) {
    case AttrType::integer:
      return "integer";
    case AttrType::boolean:
      return "boolean";
    case AttrType::enum_:
      return "enum";
    case AttrType::octetString:
      return "octetString";
    case AttrType::dateTime:
      return "dateTime";
    case AttrType::resolution:
      return "resolution";
    case AttrType::rangeOfInteger:
      return "rangeOfInteger";
    case AttrType::collection:
      return "collection";
    case AttrType::text:
      return "text";
    case AttrType::name:
      return "name";
    case AttrType::keyword:
      return "keyword";
    case AttrType::uri:
      return "uri";
    case AttrType::uriScheme:
      return "uriScheme";
    case AttrType::charset:
      return "charset";
    case AttrType::naturalLanguage:
      return "naturalLanguage";
    case AttrType::mimeMediaType:
      return "mimeMediaType";
  }
  return "";
}

std::string ToString(bool v) {
  return (v ? "true" : "false");
}

std::string ToString(int v) {
  if (v < 0) {
    // 2 x incrementation in case of (v == numeric_limit<int>::min())
    const std::string s = UnsignedToString(static_cast<size_t>(-(++v)) + 1);
    return "-" + s;
  }
  return UnsignedToString(v);
}

std::string ToString(const Resolution& v) {
  std::string s = ToString(v.xres) + "x" + ToString(v.yres);
  if (v.units == Resolution::kDotsPerInch)
    s += "dpi";
  else
    s += "dpc";
  return s;
}

std::string ToString(const RangeOfInteger& v) {
  return ("(" + ToString(v.min_value) + ":" + ToString(v.max_value) + ")");
}

std::string ToString(const DateTime& v) {
  return (ToString(v.year) + "-" + ToString(v.month) + "-" + ToString(v.day) +
          "," + ToString(v.hour) + ":" + ToString(v.minutes) + ":" +
          ToString(v.seconds) + "." + ToString(v.deci_seconds) + "," +
          std::string(1, v.UTC_direction) + ToString(v.UTC_hours) + ":" +
          ToString(v.UTC_minutes));
}

std::string ToString(const StringWithLanguage& value) {
  return value.value;
}

bool FromString(const std::string& s, bool* v) {
  if (v == nullptr)
    return false;
  if (s == "false") {
    *v = false;
    return true;
  }
  if (s == "true") {
    *v = true;
    return true;
  }
  return false;
}

// JSON-like integer format: first character may be '-', the rest must be
// digits. Leading zeroes allowed.
bool FromString(const std::string& s, int* out) {
  if (out == nullptr)
    return false;
  if (s.empty())
    return false;
  auto it = s.begin();
  int vv = 0;
  if (*it == '-') {
    ++it;
    if (it == s.end())
      return false;
    // negative number
    for (; it != s.end(); ++it) {
      if (std::numeric_limits<int>::min() / 10 > vv)
        return false;
      vv *= 10;
      if (*it < '0' || *it > '9')
        return false;
      const int d = (*it - '0');
      if (std::numeric_limits<int>::min() + d > vv)
        return false;
      vv -= d;
    }
  } else {
    // positive number
    for (; it != s.end(); ++it) {
      if (std::numeric_limits<int>::max() / 10 < vv)
        return false;
      vv *= 10;
      if (*it < '0' || *it > '9')
        return false;
      const int d = (*it - '0');
      if (std::numeric_limits<int>::max() - d < vv)
        return false;
      vv += d;
    }
  }
  *out = vv;
  return true;
}

// Final class for Attribute represents unknown attribute, i.e.: an attribute
// defined during runtime.
class UnknownAttribute : public Attribute {
 public:
  UnknownAttribute(Collection* owner, AttrName name)
      : Attribute(nullptr, name), owner_(owner) {}
  Collection* const owner_;
};

Collection* Attribute::GetOwner() const {
  if (offset_ == std::numeric_limits<int16_t>::min())
    return static_cast<const UnknownAttribute*>(this)->owner_;
  return reinterpret_cast<Collection*>(reinterpret_cast<std::intptr_t>(this) -
                                       offset_);
}

AttrType Attribute::GetType() const {
  return GetOwner()->GetAttributeDefinition(name_).ipp_type;
}

bool Attribute::IsASet() const {
  return GetOwner()->GetAttributeDefinition(name_).is_a_set;
}

AttrState Attribute::GetState() const {
  Collection* coll = GetOwner();
  if (coll->values_.count(name_))
    return AttrState::set;
  auto it = coll->states_.find(name_);
  if (it != coll->states_.end())
    return it->second;
  return AttrState::unset;
}

void Attribute::SetState(AttrState status) {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  if (status == AttrState::set) {
    ResizeAttr(&coll->values_, name_, def, 1, false);
    return;
  }
  DeleteAttr(&coll->values_, name_, def);
  if (status != AttrState::unset) {
    coll->states_[name_] = status;
  }
}

Attribute::Attribute(Collection* owner, AttrName name)
    : offset_((owner == nullptr) ? (std::numeric_limits<int16_t>::min())
                                 : (reinterpret_cast<std::intptr_t>(this) -
                                    reinterpret_cast<std::intptr_t>(owner))),
      name_(name) {
  if (owner != nullptr)
    assert(GetOwner() == owner);
}

size_t Attribute::GetSize() const {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  auto it = coll->values_.find(name_);
  if (it == coll->values_.end())
    return 0;
  if (!def.is_a_set)
    return 1;
  switch (def.cc_type) {
    case InternalType::kInteger:
      return ReadValueConstPtr<std::vector<int32_t>>(&it->second)->size();
    case InternalType::kString:
      return ReadValueConstPtr<std::vector<std::string>>(&it->second)->size();
    case InternalType::kResolution:
      return ReadValueConstPtr<std::vector<Resolution>>(&it->second)->size();
    case InternalType::kRangeOfInteger:
      return ReadValueConstPtr<std::vector<RangeOfInteger>>(&it->second)
          ->size();
    case InternalType::kDateTime:
      return ReadValueConstPtr<std::vector<DateTime>>(&it->second)->size();
    case InternalType::kStringWithLanguage:
      return ReadValueConstPtr<std::vector<StringWithLanguage>>(&it->second)
          ->size();
    case InternalType::kCollection:
      return ReadValueConstPtr<std::vector<Collection*>>(&it->second)->size();
  }
  return 0;
}

void Attribute::Resize(size_t new_size) {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  if (ResizeAttr(&coll->values_, name_, def, new_size, true)) {
    if (new_size > 0)
      coll->states_.erase(name_);
  }
}

bool Attribute::GetValue(std::string* val, size_t index) const {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  return ReadConvertValue(coll->values_, name_, def, index, val);
}

bool Attribute::GetValue(StringWithLanguage* val, size_t index) const {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  return ReadConvertValue(coll->values_, name_, def, index, val);
}

bool Attribute::GetValue(int* val, size_t index) const {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  return ReadConvertValue(coll->values_, name_, def, index, val);
}

bool Attribute::GetValue(Resolution* val, size_t index) const {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  return ReadConvertValue(coll->values_, name_, def, index, val);
}

bool Attribute::GetValue(RangeOfInteger* val, size_t index) const {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  return ReadConvertValue(coll->values_, name_, def, index, val);
}

bool Attribute::GetValue(DateTime* val, size_t index) const {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  return ReadConvertValue(coll->values_, name_, def, index, val);
}

bool Attribute::SetValue(const std::string& val, size_t index) {
  Collection* coll = GetOwner();
  return coll->SaveValue(name_, index, val);
}

bool Attribute::SetValue(const StringWithLanguage& val, size_t index) {
  Collection* coll = GetOwner();
  return coll->SaveValue(name_, index, val);
}

bool Attribute::SetValue(const int& val, size_t index) {
  Collection* coll = GetOwner();
  return coll->SaveValue(name_, index, val);
}

bool Attribute::SetValue(const Resolution& val, size_t index) {
  Collection* coll = GetOwner();
  return coll->SaveValue(name_, index, val);
}

bool Attribute::SetValue(const RangeOfInteger& val, size_t index) {
  Collection* coll = GetOwner();
  return coll->SaveValue(name_, index, val);
}

bool Attribute::SetValue(const DateTime& val, size_t index) {
  Collection* coll = GetOwner();
  return coll->SaveValue(name_, index, val);
}

Collection* Attribute::GetCollection(size_t index) {
  Collection* coll = GetOwner();
  const AttrDef def = coll->GetAttributeDefinition(name_);
  if (def.cc_type != InternalType::kCollection)
    return nullptr;
  auto it = coll->values_.find(name_);
  if (it == coll->values_.end())
    return nullptr;
  Collection* p = nullptr;
  if (def.is_a_set) {
    auto v = ReadValuePtr<std::vector<Collection*>>(&it->second);
    if (v->size() > index)
      p = *(v->data() + index);
  } else {
    p = *(ReadValuePtr<Collection*>(&it->second));
  }
  return p;
}

Collection::~Collection() {
  std::vector<AttrName> names;
  for (auto& name_value : values_)
    names.push_back(name_value.first);
  for (auto& name : names) {
    const AttrDef def = GetAttributeDefinition(name);
    DeleteAttr(&values_, name, def);
  }
  for (auto& name_attr : unknown_attributes) {
    delete static_cast<UnknownAttribute*>(name_attr.second.object);
  }
}

AttrDef Collection::GetAttributeDefinition(AttrName name) const {
  auto it2 = unknown_attributes.find(name);
  if (it2 != unknown_attributes.end())
    return it2->second.def;
  auto it = definitions_->find(name);
  if (it != definitions_->end())
    return it->second;
  return {AttrType::integer, InternalType::kInteger, false};
}

Attribute* Collection::GetAttribute(AttrName an) {
  const std::vector<Attribute*> known_attr = GetKnownAttributes();
  for (auto a : known_attr)
    if (a->GetNameAsEnum() == an)
      return a;
  auto it2 = unknown_attributes.find(an);
  if (it2 != unknown_attributes.end())
    return it2->second.object;
  return nullptr;
}

Attribute* Collection::GetAttribute(const std::string& name) {
  AttrName an = AttrName::_unknown;
  if (!FromString(name, &an)) {
    for (const auto& e : unknown_names) {
      if (e.second == name) {
        an = e.first;
        break;
      }
    }
  }
  return GetAttribute(an);
}

Attribute* Collection::AddUnknownAttribute(const std::string& name,
                                           bool is_a_set,
                                           AttrType type) {
  // name cannot be empty
  if (name.empty())
    return nullptr;
  // type must be correct
  if (ToString(type) == "")
    return nullptr;
  //
  AttrName an = AttrName::_unknown;
  if (!FromString(name, &an)) {
    for (const auto& e : unknown_names) {
      if (e.second == name)
        return nullptr;
    }
    if (unknown_attributes.empty()) {
      an = static_cast<AttrName>(std::numeric_limits<uint16_t>::max());
    } else {
      an = static_cast<AttrName>(
          static_cast<uint16_t>(unknown_attributes.begin()->first) - 1);
    }
    unknown_names[an] = name;
  } else if (GetAttribute(an) != nullptr) {
    return nullptr;
  }
  unknown_attributes[an].def.ipp_type = type;
  unknown_attributes[an].def.is_a_set = is_a_set;
  unknown_attributes[an].def.cc_type = InternalTypeForUnknownAttribute(type);
  unknown_attributes[an].object = new UnknownAttribute(this, an);
  if (type == AttrType::collection) {
    unknown_attributes[an].def.constructor = []() -> Collection* {
      return new EmptyCollection();
    };
  }
  return unknown_attributes[an].object;
}

std::vector<Attribute*> Collection::GetAllAttributes() {
  const std::vector<Attribute*> known_attr = GetKnownAttributes();
  std::vector<Attribute*> v;
  v.reserve(known_attr.size() + unknown_attributes.size());
  v.insert(v.end(), known_attr.begin(), known_attr.end());
  for (const auto& e : unknown_attributes)
    v.push_back(e.second.object);
  return v;
}

// Saves |value| to attribute |name| at position |index| in collection |coll|.
// Proper conversion is applied when needed. The attribute is also resized when
// |index| is greater than the attribute's size. |coll| cannot be nullptr. The
// function returns true if succeeds and false when one of the following occurs:
// * the attribute is not a set and |index| > 0
// * required conversion is not possible (|value| is incorrect)
template <typename ApiType>
bool Collection::SaveValue(AttrName name, size_t index, const ApiType& value) {
  const AttrDef def = GetAttributeDefinition(name);
  bool result = false;
  switch (def.cc_type) {
    case InternalType::kInteger:
      result =
          SaveValueTyped<int32_t, ApiType>(&values_, name, def, index, value);
      break;
    case InternalType::kString:
      result = SaveValueTyped<std::string, ApiType>(&values_, name, def, index,
                                                    value);
      break;
    case InternalType::kResolution:
      result = SaveValueTyped<Resolution, ApiType>(&values_, name, def, index,
                                                   value);
      break;
    case InternalType::kRangeOfInteger:
      result = SaveValueTyped<RangeOfInteger, ApiType>(&values_, name, def,
                                                       index, value);
      break;
    case InternalType::kDateTime:
      result =
          SaveValueTyped<DateTime, ApiType>(&values_, name, def, index, value);
      break;
    case InternalType::kStringWithLanguage:
      result = SaveValueTyped<StringWithLanguage, ApiType>(&values_, name, def,
                                                           index, value);
      break;
    case InternalType::kCollection:
      return false;
  }
  if (result)
    states_.erase(name);
  return result;
}

const std::map<AttrName, AttrDef> EmptyCollection::defs_;

}  // namespace ipp
