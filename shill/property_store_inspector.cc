// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store_inspector.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

PropertyStoreInspector::PropertyStoreInspector() : store_(NULL) {}

PropertyStoreInspector::PropertyStoreInspector(const PropertyStore *store)
    : store_(store) {}

PropertyStoreInspector::~PropertyStoreInspector() {}

bool PropertyStoreInspector::GetBoolProperty(const string &name, bool *value) {
  return GetProperty(name, value, &PropertyStore::GetBoolPropertiesIter);
}

bool PropertyStoreInspector::GetInt16Property(const string &name,
                                              int16 *value) {
  return GetProperty(name, value, &PropertyStore::GetInt16PropertiesIter);
}

bool PropertyStoreInspector::GetInt32Property(const string &name,
                                              int32 *value) {
  return GetProperty(name, value, &PropertyStore::GetInt32PropertiesIter);
}

bool PropertyStoreInspector::GetKeyValueStoreProperty(const string &name,
                                                      KeyValueStore *value) {
  return GetProperty(name, value,
                     &PropertyStore::GetKeyValueStorePropertiesIter);
}

bool PropertyStoreInspector::GetStringProperty(const string &name,
                                               string *value) {
  return GetProperty(name, value, &PropertyStore::GetStringPropertiesIter);
}

bool PropertyStoreInspector::GetStringmapProperty(const string &name,
                                                  map<string, string> *values) {
  return GetProperty(name, values, &PropertyStore::GetStringmapPropertiesIter);
}

bool PropertyStoreInspector::GetStringsProperty(const string &name,
                                                vector<string> *values) {
  return GetProperty(name, values, &PropertyStore::GetStringsPropertiesIter);
}

bool PropertyStoreInspector::GetUint8Property(const string &name,
                                              uint8 *value) {
  return GetProperty(name, value, &PropertyStore::GetUint8PropertiesIter);
}

bool PropertyStoreInspector::GetUint16Property(const string &name,
                                               uint16 *value) {
  return GetProperty(name, value, &PropertyStore::GetUint16PropertiesIter);
}

bool PropertyStoreInspector::GetUint32Property(const string &name,
                                               uint32 *value) {
  return GetProperty(name, value, &PropertyStore::GetUint32PropertiesIter);
}

bool PropertyStoreInspector::GetRpcIdentifierProperty(const string &name,
                                                      RpcIdentifier *value) {
  return GetProperty(name, value,
                     &PropertyStore::GetRpcIdentifierPropertiesIter);
}

template <class V>
bool PropertyStoreInspector::GetProperty(
    const string &name,
    V *value,
    ReadablePropertyConstIterator<V>(PropertyStore::*getter)() const) {
  for (ReadablePropertyConstIterator<V> it = ((*store_).*getter)();
       !it.AtEnd(); it.Advance()) {
    if (it.Key() == name) {
      if (value) {
        *value = it.value();
      }
      return true;
    }
  }
  return false;
};

}  // namespace shill
