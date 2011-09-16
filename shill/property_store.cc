// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store.h"

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/stl_util-inl.h>

#include "shill/error.h"
#include "shill/property_accessor.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

PropertyStore::PropertyStore() {}

PropertyStore::~PropertyStore() {}

bool PropertyStore::Contains(const std::string &prop) const {
  return (bool_properties_.find(prop) != bool_properties_.end() ||
          int16_properties_.find(prop) != int16_properties_.end() ||
          int32_properties_.find(prop) != int32_properties_.end() ||
          string_properties_.find(prop) != string_properties_.end() ||
          stringmap_properties_.find(prop) != stringmap_properties_.end() ||
          stringmaps_properties_.find(prop) != stringmaps_properties_.end() ||
          strintpair_properties_.find(prop) != strintpair_properties_.end() ||
          strings_properties_.find(prop) != strings_properties_.end() ||
          uint8_properties_.find(prop) != uint8_properties_.end() ||
          uint16_properties_.find(prop) != uint16_properties_.end() ||
          uint32_properties_.find(prop) != uint32_properties_.end());
}

bool PropertyStore::SetBoolProperty(const std::string& name,
                                    bool value,
                                    Error *error) {
  VLOG(2) << "Setting " << name << " as a bool.";
  bool set = (ContainsKey(bool_properties_, name) &&
              bool_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W bool.");
  return set;
}

bool PropertyStore::SetInt16Property(const std::string& name,
                                     int16 value,
                                     Error *error) {
  VLOG(2) << "Setting " << name << " as an int16.";
  bool set = (ContainsKey(int16_properties_, name) &&
              int16_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W int16.");
  return set;
}

bool PropertyStore::SetInt32Property(const std::string& name,
                                     int32 value,
                                     Error *error) {
  VLOG(2) << "Setting " << name << " as an int32.";
  bool set = (ContainsKey(int32_properties_, name) &&
              int32_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W int32.");
  return set;
}

bool PropertyStore::SetStringProperty(const std::string& name,
                                      const std::string& value,
                                      Error *error) {
  VLOG(2) << "Setting " << name << " as a string.";
  bool set = (ContainsKey(string_properties_, name) &&
              string_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W string.");
  return set;
}

bool PropertyStore::SetStringmapProperty(
    const std::string& name,
    const std::map<std::string, std::string>& values,
    Error *error) {
  VLOG(2) << "Setting " << name << " as a string map.";
  bool set = (ContainsKey(stringmap_properties_, name) &&
              stringmap_properties_[name]->Set(values));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W strmap.");
  return set;
}

bool PropertyStore::SetStringsProperty(const std::string& name,
                                       const std::vector<std::string>& values,
                                       Error *error) {
  VLOG(2) << "Setting " << name << " as a string list.";
  bool set = (ContainsKey(strings_properties_, name) &&
              strings_properties_[name]->Set(values));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W str list.");
  return set;
}

bool PropertyStore::SetUint8Property(const std::string& name,
                                     uint8 value,
                                     Error *error) {
  VLOG(2) << "Setting " << name << " as a uint8.";
  bool set = (ContainsKey(uint8_properties_, name) &&
              uint8_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W uint8.");
  return set;
}

bool PropertyStore::SetUint16Property(const std::string& name,
                                      uint16 value,
                                      Error *error) {
  VLOG(2) << "Setting " << name << " as a uint16.";
  bool set = (ContainsKey(uint16_properties_, name) &&
              uint16_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W uint16.");
  return set;
}

bool PropertyStore::SetUint32Property(const std::string& name,
                                      uint32 value,
                                      Error *error) {
  VLOG(2) << "Setting " << name << " as a uint32.";
  bool set = (ContainsKey(uint32_properties_, name) &&
              uint32_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W uint32.");
  return set;
}

PropertyConstIterator<bool> PropertyStore::GetBoolPropertiesIter() const {
  return PropertyConstIterator<bool>(bool_properties_);
}

PropertyConstIterator<int16> PropertyStore::GetInt16PropertiesIter() const {
  return PropertyConstIterator<int16>(int16_properties_);
}

PropertyConstIterator<int32> PropertyStore::GetInt32PropertiesIter() const {
  return PropertyConstIterator<int32>(int32_properties_);
}

PropertyConstIterator<std::string> PropertyStore::GetStringPropertiesIter()
    const {
  return PropertyConstIterator<std::string>(string_properties_);
}

PropertyConstIterator<Stringmap> PropertyStore::GetStringmapPropertiesIter()
    const {
  return PropertyConstIterator<Stringmap>(stringmap_properties_);
}

PropertyConstIterator<Strings> PropertyStore::GetStringsPropertiesIter() const {
  return PropertyConstIterator<Strings>(strings_properties_);
}

PropertyConstIterator<StrIntPair> PropertyStore::GetStrIntPairPropertiesIter()
    const {
  return PropertyConstIterator<StrIntPair>(strintpair_properties_);
}

PropertyConstIterator<uint8> PropertyStore::GetUint8PropertiesIter() const {
  return PropertyConstIterator<uint8>(uint8_properties_);
}

PropertyConstIterator<uint16> PropertyStore::GetUint16PropertiesIter() const {
  return PropertyConstIterator<uint16>(uint16_properties_);
}

PropertyConstIterator<uint32> PropertyStore::GetUint32PropertiesIter() const {
  return PropertyConstIterator<uint32>(uint32_properties_);
}

void PropertyStore::RegisterBool(const string &name, bool *prop) {
  bool_properties_[name] = BoolAccessor(new PropertyAccessor<bool>(prop));
}

void PropertyStore::RegisterConstBool(const string &name, const bool *prop) {
  bool_properties_[name] = BoolAccessor(new ConstPropertyAccessor<bool>(prop));
}

void PropertyStore::RegisterInt16(const string &name, int16 *prop) {
  int16_properties_[name] = Int16Accessor(new PropertyAccessor<int16>(prop));
}

void PropertyStore::RegisterConstInt16(const string &name, const int16 *prop) {
  int16_properties_[name] =
      Int16Accessor(new ConstPropertyAccessor<int16>(prop));
}

void PropertyStore::RegisterInt32(const string &name, int32 *prop) {
  int32_properties_[name] = Int32Accessor(new PropertyAccessor<int32>(prop));
}

void PropertyStore::RegisterConstInt32(const string &name, const int32 *prop) {
  int32_properties_[name] =
      Int32Accessor(new ConstPropertyAccessor<int32>(prop));
}

void PropertyStore::RegisterString(const string &name, string *prop) {
  string_properties_[name] = StringAccessor(new PropertyAccessor<string>(prop));
}

void PropertyStore::RegisterConstString(const string &name,
                                        const string *prop) {
  string_properties_[name] =
      StringAccessor(new ConstPropertyAccessor<string>(prop));
}

void PropertyStore::RegisterStringmap(const string &name, Stringmap *prop) {
  stringmap_properties_[name] =
      StringmapAccessor(new PropertyAccessor<Stringmap>(prop));
}

void PropertyStore::RegisterConstStringmap(const string &name,
                                           const Stringmap *prop) {
  stringmap_properties_[name] =
      StringmapAccessor(new ConstPropertyAccessor<Stringmap>(prop));
}

void PropertyStore::RegisterStrings(const string &name, Strings *prop) {
  strings_properties_[name] =
      StringsAccessor(new PropertyAccessor<Strings>(prop));
}

void PropertyStore::RegisterConstStrings(const string &name,
                                         const Strings *prop) {
  strings_properties_[name] =
      StringsAccessor(new ConstPropertyAccessor<Strings>(prop));
}

void PropertyStore::RegisterUint8(const string &name, uint8 *prop) {
  uint8_properties_[name] = Uint8Accessor(new PropertyAccessor<uint8>(prop));
}

void PropertyStore::RegisterConstUint8(const string &name, const uint8 *prop) {
  uint8_properties_[name] =
      Uint8Accessor(new ConstPropertyAccessor<uint8>(prop));
}

void PropertyStore::RegisterUint16(const std::string &name, uint16 *prop) {
  uint16_properties_[name] = Uint16Accessor(new PropertyAccessor<uint16>(prop));
}

void PropertyStore::RegisterConstUint16(const string &name,
                                        const uint16 *prop) {
  uint16_properties_[name] =
      Uint16Accessor(new ConstPropertyAccessor<uint16>(prop));
}

void PropertyStore::RegisterDerivedBool(const std::string &name,
                                        const BoolAccessor &accessor) {
  bool_properties_[name] = accessor;
}

void PropertyStore::RegisterDerivedString(const std::string &name,
                                          const StringAccessor &accessor) {
  string_properties_[name] = accessor;
}

void PropertyStore::RegisterDerivedStrings(const std::string &name,
                                           const StringsAccessor &accessor) {
  strings_properties_[name] = accessor;
}

void PropertyStore::RegisterDerivedStringmaps(const std::string &name,
                                              const StringmapsAccessor &acc) {
  stringmaps_properties_[name] = acc;
}

void PropertyStore::RegisterDerivedStrIntPair(const std::string &name,
                                              const StrIntPairAccessor &acc) {
  strintpair_properties_[name] = acc;
}

}  // namespace shill
