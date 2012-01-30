// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
  virtual bool SetBoolProperty(const std::string &name,
                               bool value,
                               Error *error);

  virtual bool SetInt16Property(const std::string &name,
                                int16 value,
                                Error *error);

  virtual bool SetInt32Property(const std::string &name,
                                int32 value,
                                Error *error);

  virtual bool SetStringProperty(const std::string &name,
                                 const std::string &value,
                                 Error *error);

  virtual bool SetStringmapProperty(
      const std::string &name,
      const std::map<std::string, std::string> &values,
      Error *error);

  virtual bool SetStringsProperty(const std::string &name,
                                  const std::vector<std::string> &values,
                                  Error *error);

  virtual bool SetUint8Property(const std::string &name,
                                uint8 value,
                                Error *error);

  virtual bool SetUint16Property(const std::string &name,
                                 uint16 value,
                                 Error *error);

  virtual bool SetUint32Property(const std::string &name,
                                 uint32 value,
                                 Error *error);

  // Clearing a property resets it to its "factory" value. This value
  // is generally the value that it (the property) had when it was
  // registered with PropertyStore.
  //
  // The exception to this rule is write-only derived properties. For
  // such properties, the property owner explicitly provides a
  // "factory" value at registration time. This is necessary because
  // PropertyStore can't read the current value at registration time.
  //
  // |name| is the key used to access the property. If the property
  // cannot be cleared, |error| is set, and the method returns false.
  // Otherwrise, |error| is unchanged, and the method returns true.
  virtual bool ClearProperty(const std::string &name, Error *error);

  // We do not provide methods for reading individual properties,
  // because we don't need them to implement the flimflam API. (The flimflam
  // API only allows fetching all properties at once -- not individual
  // properties.)

  // Accessors for iterators over property maps. Useful for dumping all
  // properties.
  ReadablePropertyConstIterator<bool> GetBoolPropertiesIter() const;
  ReadablePropertyConstIterator<int16> GetInt16PropertiesIter() const;
  ReadablePropertyConstIterator<int32> GetInt32PropertiesIter() const;
  ReadablePropertyConstIterator<KeyValueStore> GetKeyValueStorePropertiesIter(
      ) const;
  ReadablePropertyConstIterator<std::string> GetStringPropertiesIter() const;
  ReadablePropertyConstIterator<Stringmap> GetStringmapPropertiesIter() const;
  ReadablePropertyConstIterator<Stringmaps> GetStringmapsPropertiesIter() const;
  ReadablePropertyConstIterator<Strings> GetStringsPropertiesIter() const;
  ReadablePropertyConstIterator<uint8> GetUint8PropertiesIter() const;
  ReadablePropertyConstIterator<uint16> GetUint16PropertiesIter() const;
  ReadablePropertyConstIterator<uint32> GetUint32PropertiesIter() const;

  // Methods for registering a property.
  //
  // It is permitted to re-register a property (in which case the old
  // binding is forgotten). However, the newly bound object must be of
  // the same type.
  //
  // Note that types do not encode read-write permission.  Hence, it
  // is possible to change permissions by rebinding a property to the
  // same object.
  //
  // (Corollary of the rebinding-to-same-type restriction: a
  // PropertyStore cannot hold two properties of the same name, but
  // differing types.)
  void RegisterBool(const std::string &name, bool *prop);
  void RegisterConstBool(const std::string &name, const bool *prop);
  void RegisterWriteOnlyBool(const std::string &name, bool *prop);
  void RegisterInt16(const std::string &name, int16 *prop);
  void RegisterConstInt16(const std::string &name, const int16 *prop);
  void RegisterWriteOnlyInt16(const std::string &name, int16 *prop);
  void RegisterInt32(const std::string &name, int32 *prop);
  void RegisterConstInt32(const std::string &name, const int32 *prop);
  void RegisterWriteOnlyInt32(const std::string &name, int32 *prop);
  void RegisterUint32(const std::string &name, uint32 *prop);
  void RegisterString(const std::string &name, std::string *prop);
  void RegisterConstString(const std::string &name, const std::string *prop);
  void RegisterWriteOnlyString(const std::string &name, std::string *prop);
  void RegisterStringmap(const std::string &name, Stringmap *prop);
  void RegisterConstStringmap(const std::string &name, const Stringmap *prop);
  void RegisterWriteOnlyStringmap(const std::string &name, Stringmap *prop);
  void RegisterStringmaps(const std::string &name, Stringmaps *prop);
  void RegisterConstStringmaps(const std::string &name, const Stringmaps *prop);
  void RegisterWriteOnlyStringmaps(const std::string &name, Stringmaps *prop);
  void RegisterStrings(const std::string &name, Strings *prop);
  void RegisterConstStrings(const std::string &name, const Strings *prop);
  void RegisterWriteOnlyStrings(const std::string &name, Strings *prop);
  void RegisterUint8(const std::string &name, uint8 *prop);
  void RegisterConstUint8(const std::string &name, const uint8 *prop);
  void RegisterWriteOnlyUint8(const std::string &name, uint8 *prop);
  void RegisterUint16(const std::string &name, uint16 *prop);
  void RegisterConstUint16(const std::string &name, const uint16 *prop);
  void RegisterWriteOnlyUint16(const std::string &name, uint16 *prop);

  void RegisterDerivedBool(const std::string &name,
                           const BoolAccessor &accessor);
  void RegisterDerivedInt32(const std::string &name,
                            const Int32Accessor &accessor);
  void RegisterDerivedKeyValueStore(const std::string &name,
                                    const KeyValueStoreAccessor &accessor);
  void RegisterDerivedString(const std::string &name,
                             const StringAccessor &accessor);
  void RegisterDerivedStringmaps(const std::string &name,
                                 const StringmapsAccessor &accessor);
  void RegisterDerivedStrings(const std::string &name,
                              const StringsAccessor &accessor);
  void RegisterDerivedUint16(const std::string &name,
                             const Uint16Accessor &accessor);

 private:
  template <class V>
  bool SetProperty(
      const std::string &name,
      const V &value,
      Error *error,
      std::map< std::string, std::tr1::shared_ptr< AccessorInterface<V> > > &,
      const std::string &value_type_english);

  // These are std::maps instead of something cooler because the common
  // operation is iterating through them and returning all properties.
  std::map<std::string, BoolAccessor> bool_properties_;
  std::map<std::string, Int16Accessor> int16_properties_;
  std::map<std::string, Int32Accessor> int32_properties_;
  std::map<std::string, KeyValueStoreAccessor> key_value_store_properties_;
  std::map<std::string, StringAccessor> string_properties_;
  std::map<std::string, StringmapAccessor> stringmap_properties_;
  std::map<std::string, StringmapsAccessor> stringmaps_properties_;
  std::map<std::string, StringsAccessor> strings_properties_;
  std::map<std::string, Uint8Accessor> uint8_properties_;
  std::map<std::string, Uint16Accessor> uint16_properties_;
  std::map<std::string, Uint32Accessor> uint32_properties_;

  DISALLOW_COPY_AND_ASSIGN(PropertyStore);
};

}  // namespace shill

#endif  // SHILL_PROPERTY_STORE_
