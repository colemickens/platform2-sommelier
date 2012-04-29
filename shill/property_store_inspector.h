// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_STORE_INSPECTOR_
#define SHILL_PROPERTY_STORE_INSPECTOR_

#ifndef UNIT_TEST
#error "Do not use PropertyStoreInspector in non-test code."
#endif

#include "shill/property_store.h"

namespace shill {

// This class supplies functions to unit tests for inspecting a PropertyStore
// class to get single attributes.  We never need to do this in the real
// PropertyStore, since the only interface to it over the control API is
// "GetProperties" which iterates over the entire store's properties.
//
// This code is done inefficiently -- if we were ever meant to retrieve
// single properties in production code, we'd access the map directly.
// For this reason, this class should only be used in unit tests.
class PropertyStoreInspector {
 public:
  PropertyStoreInspector();
  explicit PropertyStoreInspector(const PropertyStore *store);
  virtual ~PropertyStoreInspector();

  const PropertyStore *store() const { return store_; }
  void set_store(const PropertyStore *store) { store_ = store; }

  // Methods to allow the getting, of properties stored in the referenced
  // |store_| by name.  Upon success, these methods return true, return
  // the property value in |value|, and leave |error| untouched.  Upon
  // failure, they return false and if non-NULL, |error| is set appropriately.
  // The value for |error| may be returned by the property getter itself, or
  // if the property is not found in the store, these functions return
  // Error::kNotFound.
  bool GetBoolProperty(const std::string &name, bool *value, Error *error);
  bool GetInt16Property(const std::string &name, int16 *value, Error *error);
  bool GetInt32Property(const std::string &name, int32 *value, Error *error);
  bool GetKeyValueStoreProperty(const std::string &name,
                                KeyValueStore *value,
                                Error *error);
  bool GetStringProperty(const std::string &name,
                         std::string *value,
                         Error *error);
  bool GetStringmapProperty(const std::string &name,
                            std::map<std::string, std::string> *values,
                            Error *error);
  bool GetStringsProperty(const std::string &name,
                          std::vector<std::string> *values,
                          Error *error);
  bool GetUint8Property(const std::string &name, uint8 *value, Error *error);
  bool GetUint16Property(const std::string &name, uint16 *value, Error *error);
  bool GetUint32Property(const std::string &name, uint32 *value, Error *error);
  bool GetRpcIdentifierProperty(const std::string &name,
                                RpcIdentifier *value,
                                Error *error);

  // Methods to test the whether a property is enumerable for reading -- rather
  // specifically that trying to do so would either succeed or return
  // an error other than Error::kNotFound.  Therefore, this function will
  // return true for both successfully readable properties or properties that
  // return a different error for their getter (e.g., Error:kPermissionDenied).
  bool ContainsBoolProperty(const std::string &name);
  bool ContainsInt16Property(const std::string &name);
  bool ContainsInt32Property(const std::string &name);
  bool ContainsKeyValueStoreProperty(const std::string &name);
  bool ContainsStringProperty(const std::string &name);
  bool ContainsStringmapProperty(const std::string &name);
  bool ContainsStringsProperty(const std::string &name);
  bool ContainsUint8Property(const std::string &name);
  bool ContainsUint16Property(const std::string &name);
  bool ContainsUint32Property(const std::string &name);
  bool ContainsRpcIdentifierProperty(const std::string &name);

 private:
  template <class V>
  bool GetProperty(
      const std::string &name,
      V *value,
      Error *error,
      ReadablePropertyConstIterator<V>(PropertyStore::*getter)() const);

  template <class V>
  bool ContainsProperty(
      const std::string &name,
      ReadablePropertyConstIterator<V>(PropertyStore::*getter)() const);

  const PropertyStore *store_;

  DISALLOW_COPY_AND_ASSIGN(PropertyStoreInspector);
};

}  // namespace shill


#endif  // SHILL_PROPERTY_STORE_INSPECTOR_
