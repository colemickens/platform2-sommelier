// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store.h"

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>

#include "shill/error.h"
#include "shill/property_accessor.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

PropertyStore::PropertyStore() {}

PropertyStore::~PropertyStore() {}

bool PropertyStore::Contains(const std::string &property) {
  return (bool_properties_.find(property) != bool_properties_.end() ||
          int16_properties_.find(property) != int16_properties_.end() ||
          int32_properties_.find(property) != int32_properties_.end() ||
          string_properties_.find(property) != string_properties_.end() ||
          stringmap_properties_.find(property) != stringmap_properties_.end() ||
          strings_properties_.find(property) != strings_properties_.end() ||
          uint8_properties_.find(property) != uint8_properties_.end() ||
          uint16_properties_.find(property) != uint16_properties_.end() ||
          uint32_properties_.find(property) != uint32_properties_.end());
}

bool PropertyStore::SetBoolProperty(const std::string& name,
                                    bool value,
                                    Error *error) {
  return ReturnError(name, error);
}

bool PropertyStore::SetInt16Property(const std::string& name,
                                     int16 value,
                                     Error *error) {
  return ReturnError(name, error);
}

bool PropertyStore::SetInt32Property(const std::string& name,
                                     int32 value,
                                     Error *error) {
  return ReturnError(name, error);
}

bool PropertyStore::SetStringProperty(const std::string& name,
                                      const std::string& value,
                                      Error *error) {
  return ReturnError(name, error);
}

bool PropertyStore::SetStringmapProperty(
    const std::string& name,
    const std::map<std::string, std::string>& values,
    Error *error) {
  return ReturnError(name, error);
}

bool PropertyStore::SetStringsProperty(const std::string& name,
                                       const std::vector<std::string>& values,
                                       Error *error) {
  return ReturnError(name, error);
}

bool PropertyStore::SetUint8Property(const std::string& name,
                                     uint8 value,
                                     Error *error) {
  return ReturnError(name, error);
}

bool PropertyStore::SetUint16Property(const std::string& name,
                                      uint16 value,
                                      Error *error) {
  return ReturnError(name, error);
}

bool PropertyStore::SetUint32Property(const std::string& name,
                                      uint32 value,
                                      Error *error) {
  return ReturnError(name, error);
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

void PropertyStore::RegisterStringmap(const string &name,
                                      map<string, string> *prop) {
  stringmap_properties_[name] =
      StringmapAccessor(new PropertyAccessor<map<string, string> >(prop));
}

void PropertyStore::RegisterConstStringmap(const string &name,
                                           const map<string, string> *prop) {
  stringmap_properties_[name] =
      StringmapAccessor(new ConstPropertyAccessor<map<string, string> >(prop));
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

bool PropertyStore::ReturnError(const std::string& name, Error *error) {
  NOTIMPLEMENTED() << name << " is not writable.";
  if (error)
    error->Populate(Error::kInvalidArguments, name + " is not writable.");
  return false;
}

}  // namespace shill
