// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_STORE_INSPECTOR_H_
#define SHILL_PROPERTY_STORE_INSPECTOR_H_

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

  // Methods to allow the getting of properties stored in the referenced
  // |store_| by name. Upon success, these methods return true and return the
  // property value in |value| if non-NULL. Upon failure, they return false and
  // leave |value| untouched.
  bool GetBoolProperty(const std::string &name, bool *value);
  bool GetInt16Property(const std::string &name, int16 *value);
  bool GetInt32Property(const std::string &name, int32 *value);
  bool GetKeyValueStoreProperty(const std::string &name, KeyValueStore *value);
  bool GetStringProperty(const std::string &name, std::string *value);
  bool GetStringmapProperty(const std::string &name,
                            std::map<std::string, std::string> *values);
  bool GetStringsProperty(const std::string &name,
                          std::vector<std::string> *values);
  bool GetUint8Property(const std::string &name, uint8 *value);
  bool GetUint16Property(const std::string &name, uint16 *value);
  bool GetUint32Property(const std::string &name, uint32 *value);
  bool GetRpcIdentifierProperty(const std::string &name, RpcIdentifier *value);

 private:
  template <class V>
  bool GetProperty(
      const std::string &name,
      V *value,
      ReadablePropertyConstIterator<V>(PropertyStore::*getter)() const);

  const PropertyStore *store_;

  DISALLOW_COPY_AND_ASSIGN(PropertyStoreInspector);
};

}  // namespace shill


#endif  // SHILL_PROPERTY_STORE_INSPECTOR_H_
