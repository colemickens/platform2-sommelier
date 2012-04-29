// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store_inspector.h"

#include "shill/error.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

PropertyStoreInspector::PropertyStoreInspector() {}

PropertyStoreInspector::PropertyStoreInspector(const PropertyStore *store)
    : store_(store) {}

PropertyStoreInspector::~PropertyStoreInspector() {}

bool PropertyStoreInspector::GetBoolProperty(const string &name,
                                             bool *value,
                                             Error *error) {
  return GetProperty(name, value, error, &PropertyStore::GetBoolPropertiesIter);
}

bool PropertyStoreInspector::GetInt16Property(const string &name,
                                              int16 *value,
                                              Error *error) {
  return GetProperty(name, value, error,
                     &PropertyStore::GetInt16PropertiesIter);
}

bool PropertyStoreInspector::GetInt32Property(const string &name,
                                              int32 *value,
                                              Error *error) {
  return GetProperty(name, value, error,
                     &PropertyStore::GetInt32PropertiesIter);
}

bool PropertyStoreInspector::GetKeyValueStoreProperty(const string &name,
                                                      KeyValueStore *value,
                                                      Error *error) {
  return GetProperty(name, value, error,
                     &PropertyStore::GetKeyValueStorePropertiesIter);
}

bool PropertyStoreInspector::GetStringProperty(const string &name,
                                               string *value,
                                               Error *error) {
  return GetProperty(name, value, error,
                     &PropertyStore::GetStringPropertiesIter);
}

bool PropertyStoreInspector::GetStringmapProperty(const string &name,
                                                  map<string, string> *values,
                                                  Error *error) {
  return GetProperty(name, values, error,
                     &PropertyStore::GetStringmapPropertiesIter);
}

bool PropertyStoreInspector::GetStringsProperty(const string &name,
                                                vector<string> *values,
                                                Error *error) {
  return GetProperty(name, values, error,
                     &PropertyStore::GetStringsPropertiesIter);
}

bool PropertyStoreInspector::GetUint8Property(const string &name,
                                              uint8 *value,
                                              Error *error) {
  return GetProperty(name, value, error,
                     &PropertyStore::GetUint8PropertiesIter);
}

bool PropertyStoreInspector::GetUint16Property(const string &name,
                                               uint16 *value,
                                               Error *error) {
  return GetProperty(name, value, error,
                     &PropertyStore::GetUint16PropertiesIter);
}

bool PropertyStoreInspector::GetUint32Property(const string &name,
                                               uint32 *value,
                                               Error *error) {
  return GetProperty(name, value, error,
                     &PropertyStore::GetUint32PropertiesIter);
}

bool PropertyStoreInspector::GetRpcIdentifierProperty(const string &name,
                                                      RpcIdentifier *value,
                                                      Error *error) {
  return GetProperty(name, value, error,
                     &PropertyStore::GetRpcIdentifierPropertiesIter);
}

bool PropertyStoreInspector::ContainsBoolProperty(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetBoolPropertiesIter);
}

bool PropertyStoreInspector::ContainsInt16Property(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetInt16PropertiesIter);
}

bool PropertyStoreInspector::ContainsInt32Property(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetInt32PropertiesIter);
}

bool PropertyStoreInspector::ContainsKeyValueStoreProperty(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetKeyValueStorePropertiesIter);
}

bool PropertyStoreInspector::ContainsStringProperty(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetStringPropertiesIter);
}

bool PropertyStoreInspector::ContainsStringmapProperty(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetStringmapPropertiesIter);
}

bool PropertyStoreInspector::ContainsStringsProperty(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetStringsPropertiesIter);
}

bool PropertyStoreInspector::ContainsUint8Property(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetUint8PropertiesIter);
}

bool PropertyStoreInspector::ContainsUint16Property(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetUint16PropertiesIter);
}

bool PropertyStoreInspector::ContainsUint32Property(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetUint32PropertiesIter);
}

bool PropertyStoreInspector::ContainsRpcIdentifierProperty(const string &name) {
  return ContainsProperty(name, &PropertyStore::GetRpcIdentifierPropertiesIter);
}

template <class V>
bool PropertyStoreInspector::GetProperty(
    const string &name,
    V *value,
    Error *error,
    ReadablePropertyConstIterator<V>(PropertyStore::*getter)() const) {
  ReadablePropertyConstIterator<V> it = ((*store_).*getter)();
  for ( ; !it.AtEnd(); it.Advance()) {
    if (it.Key() == name) {
      *value = it.Value(error);
      return error->IsSuccess();
    }
  }
  error->Populate(Error::kNotFound);
  return false;
};

template <class V>
bool PropertyStoreInspector::ContainsProperty(
      const std::string &name,
      ReadablePropertyConstIterator<V>(PropertyStore::*getter)() const) {
  V value;
  Error error;
  GetProperty(name, &value, &error, getter);
  return error.type() != Error::kNotFound;
}

}  // namespace shill
