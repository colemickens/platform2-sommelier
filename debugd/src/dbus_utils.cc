// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/dbus_utils.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>

using base::DictionaryValue;
using base::FundamentalValue;
using base::ListValue;
using base::StringValue;
using base::Value;

namespace {

bool DBusMessageIterToValue(DBus::MessageIter* iter, Value** result);

bool DBusMessageIterToPrimitiveValue(DBus::MessageIter* iter, Value** result) {
  DBus::MessageIter subiter;
  switch (iter->type()) {
    case DBUS_TYPE_BYTE:
      *result = new FundamentalValue(iter->get_byte());
      return true;
    case DBUS_TYPE_BOOLEAN:
      *result = new FundamentalValue(iter->get_bool());
      return true;
    case DBUS_TYPE_INT16:
      *result = new FundamentalValue(iter->get_int16());
      return true;
    case DBUS_TYPE_UINT16:
      *result = new FundamentalValue(iter->get_uint16());
      return true;
    case DBUS_TYPE_INT32:
      *result = new FundamentalValue(iter->get_int32());
      return true;
    case DBUS_TYPE_UINT32:
      *result = new StringValue(base::UintToString(iter->get_uint32()));
      return true;
    case DBUS_TYPE_INT64:
      *result = new StringValue(base::Int64ToString(iter->get_int64()));
      return true;
    case DBUS_TYPE_UINT64:
      *result = new StringValue(base::Uint64ToString(iter->get_uint64()));
      return true;
    case DBUS_TYPE_DOUBLE:
      *result = new FundamentalValue(iter->get_double());
      return true;
    case DBUS_TYPE_STRING:
      *result = new StringValue(iter->get_string());
      return true;
    case DBUS_TYPE_OBJECT_PATH:
      *result = new StringValue(iter->get_path());
      return true;
    case DBUS_TYPE_SIGNATURE:
      *result = new StringValue(iter->get_signature());
      return true;
    case DBUS_TYPE_UNIX_FD:
      *result = new FundamentalValue(iter->get_int32());
      return true;
    case DBUS_TYPE_VARIANT:
      subiter = iter->recurse();
      return DBusMessageIterToValue(&subiter, result);
    default:
      LOG(ERROR) << "Unhandled primitive type: " << iter->type();
      return false;
  }
}

bool DBusMessageIterToArrayValue(DBus::MessageIter* iter, Value** result) {
  // For an array, create an empty ListValue, then recurse into it.
  ListValue* lv = new ListValue();
  while (!iter->at_end()) {
    Value* subvalue = NULL;
    bool r = DBusMessageIterToValue(iter, &subvalue);
    if (!r) {
      delete lv;
      return false;
    }
    lv->Append(subvalue);
    (*iter)++;
  }
  *result = lv;
  return true;
}

bool DBusMessageIterToDictValue(DBus::MessageIter* iter, Value** result) {
  // For a dict, create an empty DictionaryValue, then walk the subcontainers
  // with the key-value pairs, adding them to the dict.
  DictionaryValue* dv = new DictionaryValue();
  while (!iter->at_end()) {
    DBus::MessageIter subiter = iter->recurse();
    Value* key = NULL;
    Value* value = NULL;
    bool r = DBusMessageIterToValue(&subiter, &key);
    if (!r || key->GetType() != Value::TYPE_STRING) {
      delete dv;
      return false;
    }
    subiter++;
    r = DBusMessageIterToValue(&subiter, &value);
    if (!r) {
      delete key;
      delete dv;
      return false;
    }
    std::string keystr;
    key->GetAsString(&keystr);
    dv->Set(keystr, value);
    delete key;
    (*iter)++;
  }
  *result = dv;
  return true;
}

bool DBusMessageIterToValue(DBus::MessageIter* iter, Value** result) {
  if (iter->at_end()) {
    *result = NULL;
    return true;
  }
  if (!iter->is_array() && !iter->is_dict()) {
    // Primitive type. Stash it in result directly and return.
    return DBusMessageIterToPrimitiveValue(iter, result);
  }
  if (iter->is_dict()) {
    DBus::MessageIter subiter = iter->recurse();
    return DBusMessageIterToDictValue(&subiter, result);
  }
  if (iter->is_array()) {
    DBus::MessageIter subiter = iter->recurse();
    return DBusMessageIterToArrayValue(&subiter, result);
  }
  return false;
}

}  // namespace

bool debugd::DBusMessageToValue(const DBus::Message& message, Value** v) {
  DBus::MessageIter r = message.reader();
  return DBusMessageIterToArrayValue(&r, v);
}

bool debugd::DBusPropertyMapToValue(
    const std::map<std::string, DBus::Variant>& properties, Value** v) {
  DictionaryValue* dv = new DictionaryValue();
  std::map<std::string, DBus::Variant>::const_iterator it;
  for (it = properties.begin(); it != properties.end(); ++it) {
    Value* v = NULL;
    // make a copy so we can take a reference to the MessageIter
    DBus::MessageIter reader = it->second.reader();
    bool r = DBusMessageIterToValue(&reader, &v);
    if (!r) {
      delete dv;
      return false;
    }
    dv->Set(it->first, v);
  }
  *v = dv;
  return true;
}
