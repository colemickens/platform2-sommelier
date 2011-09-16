// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_STORE_
#define SHILL_PROPERTY_STORE_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>

#include "shill/accessor_interface.h"
#include "shill/property_iterator.h"

namespace shill {

class Error;

class PropertyStore {
 public:
  PropertyStore();
  virtual ~PropertyStore();

  virtual bool Contains(const std::string& property) const;

  // Methods to allow the setting, by name, of properties stored in this object.
  // The property names are declared in chromeos/dbus/service_constants.h,
  // so that they may be shared with libcros.
  // Upon success, these methods return true and leave |error| untouched.
  // Upon failure, they return false and set |error| appropriately, if it
  // is non-NULL.
  virtual bool SetBoolProperty(const std::string& name,
                               bool value,
                               Error *error);

  virtual bool SetInt16Property(const std::string& name,
                                int16 value,
                                Error *error);

  virtual bool SetInt32Property(const std::string& name,
                                int32 value,
                                Error *error);

  virtual bool SetStringProperty(const std::string& name,
                                 const std::string& value,
                                 Error *error);

  virtual bool SetStringmapProperty(
      const std::string& name,
      const std::map<std::string, std::string>& values,
      Error *error);

  virtual bool SetStringsProperty(const std::string& name,
                                  const std::vector<std::string>& values,
                                  Error *error);

  virtual bool SetUint8Property(const std::string& name,
                                uint8 value,
                                Error *error);

  virtual bool SetUint16Property(const std::string& name,
                                 uint16 value,
                                 Error *error);

  virtual bool SetUint32Property(const std::string& name,
                                 uint32 value,
                                 Error *error);

  // Accessors for iterators over property maps.
  PropertyConstIterator<bool> GetBoolPropertiesIter() const;
  PropertyConstIterator<int16> GetInt16PropertiesIter() const;
  PropertyConstIterator<int32> GetInt32PropertiesIter() const;
  PropertyConstIterator<std::string> GetStringPropertiesIter() const;
  PropertyConstIterator<Stringmap> GetStringmapPropertiesIter() const;
  PropertyConstIterator<Stringmaps> GetStringmapsPropertiesIter() const;
  PropertyConstIterator<StrIntPair> GetStrIntPairPropertiesIter() const;
  PropertyConstIterator<Strings> GetStringsPropertiesIter() const;
  PropertyConstIterator<uint8> GetUint8PropertiesIter() const;
  PropertyConstIterator<uint16> GetUint16PropertiesIter() const;
  PropertyConstIterator<uint32> GetUint32PropertiesIter() const;

  void RegisterBool(const std::string &name, bool *prop);
  void RegisterConstBool(const std::string &name, const bool *prop);
  void RegisterInt16(const std::string &name, int16 *prop);
  void RegisterConstInt16(const std::string &name, const int16 *prop);
  void RegisterInt32(const std::string &name, int32 *prop);
  void RegisterConstInt32(const std::string &name, const int32 *prop);
  void RegisterString(const std::string &name, std::string *prop);
  void RegisterConstString(const std::string &name, const std::string *prop);
  void RegisterStringmap(const std::string &name, Stringmap *prop);
  void RegisterConstStringmap(const std::string &name, const Stringmap *prop);
  void RegisterStrings(const std::string &name, Strings *prop);
  void RegisterConstStrings(const std::string &name, const Strings *prop);
  void RegisterUint8(const std::string &name, uint8 *prop);
  void RegisterConstUint8(const std::string &name, const uint8 *prop);
  void RegisterUint16(const std::string &name, uint16 *prop);
  void RegisterConstUint16(const std::string &name, const uint16 *prop);

  void RegisterDerivedBool(const std::string &name,
                           const BoolAccessor &accessor);
  void RegisterDerivedString(const std::string &name,
                             const StringAccessor &accessor);
  void RegisterDerivedStringmaps(const std::string &name,
                                 const StringmapsAccessor &accessor);
  void RegisterDerivedStrings(const std::string &name,
                              const StringsAccessor &accessor);
  void RegisterDerivedStrIntPair(const std::string &name,
                                 const StrIntPairAccessor &accessor);

 private:
  // These are std::maps instead of something cooler because the common
  // operation is iterating through them and returning all properties.
  std::map<std::string, BoolAccessor> bool_properties_;
  std::map<std::string, Int16Accessor> int16_properties_;
  std::map<std::string, Int32Accessor> int32_properties_;
  std::map<std::string, StringAccessor> string_properties_;
  std::map<std::string, StringmapAccessor> stringmap_properties_;
  std::map<std::string, StringmapsAccessor> stringmaps_properties_;
  std::map<std::string, StringsAccessor> strings_properties_;
  std::map<std::string, StrIntPairAccessor> strintpair_properties_;
  std::map<std::string, Uint8Accessor> uint8_properties_;
  std::map<std::string, Uint16Accessor> uint16_properties_;
  std::map<std::string, Uint32Accessor> uint32_properties_;

  DISALLOW_COPY_AND_ASSIGN(PropertyStore);
};

}  // namespace shill

#endif  // SHILL_PROPERTY_STORE_
