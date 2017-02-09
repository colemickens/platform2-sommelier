// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/dbus_utils.h"

#include <utility>

#include <base/logging.h>
#include <base/memory/ptr_util.h>
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

bool DBusMessageIterToValue(DBus::MessageIter* iter,
                            std::unique_ptr<Value>* result);

bool DBusMessageIterToPrimitiveValue(DBus::MessageIter* iter,
                                     std::unique_ptr<Value>* result) {
  DBus::MessageIter subiter;
  switch (iter->type()) {
    case DBUS_TYPE_BYTE:
      *result = base::MakeUnique<FundamentalValue>(iter->get_byte());
      return true;
    case DBUS_TYPE_BOOLEAN:
      *result = base::MakeUnique<FundamentalValue>(iter->get_bool());
      return true;
    case DBUS_TYPE_INT16:
      *result = base::MakeUnique<FundamentalValue>(iter->get_int16());
      return true;
    case DBUS_TYPE_UINT16:
      *result = base::MakeUnique<FundamentalValue>(iter->get_uint16());
      return true;
    case DBUS_TYPE_INT32:
      *result = base::MakeUnique<FundamentalValue>(iter->get_int32());
      return true;
    case DBUS_TYPE_UINT32:
      *result =
          base::MakeUnique<StringValue>(base::UintToString(iter->get_uint32()));
      return true;
    case DBUS_TYPE_INT64:
      *result =
          base::MakeUnique<StringValue>(base::Int64ToString(iter->get_int64()));
      return true;
    case DBUS_TYPE_UINT64:
      *result = base::MakeUnique<StringValue>(
          base::Uint64ToString(iter->get_uint64()));
      return true;
    case DBUS_TYPE_DOUBLE:
      *result = base::MakeUnique<FundamentalValue>(iter->get_double());
      return true;
    case DBUS_TYPE_STRING:
      *result = base::MakeUnique<StringValue>(iter->get_string());
      return true;
    case DBUS_TYPE_OBJECT_PATH:
      *result = base::MakeUnique<StringValue>(iter->get_path());
      return true;
    case DBUS_TYPE_SIGNATURE:
      *result = base::MakeUnique<StringValue>(iter->get_signature());
      return true;
    case DBUS_TYPE_UNIX_FD:
      *result = base::MakeUnique<FundamentalValue>(iter->get_int32());
      return true;
    case DBUS_TYPE_VARIANT:
      subiter = iter->recurse();
      return DBusMessageIterToValue(&subiter, result);
    default:
      LOG(ERROR) << "Unhandled primitive type: " << iter->type();
      return false;
  }
}

bool DBusMessageIterToArrayValue(DBus::MessageIter* iter,
                                 std::unique_ptr<Value>* result) {
  // For an array, create an empty ListValue, then recurse into it.
  auto lv = base::MakeUnique<ListValue>();
  while (!iter->at_end()) {
    std::unique_ptr<Value> subvalue;
    bool r = DBusMessageIterToValue(iter, &subvalue);
    if (!r) {
      return false;
    }
    lv->Append(std::move(subvalue));
    (*iter)++;
  }
  *result = std::move(lv);
  return true;
}

bool DBusMessageIterToDictValue(DBus::MessageIter* iter,
                                std::unique_ptr<Value>* result) {
  // For a dict, create an empty DictionaryValue, then walk the subcontainers
  // with the key-value pairs, adding them to the dict.
  auto dv = base::MakeUnique<DictionaryValue>();
  while (!iter->at_end()) {
    DBus::MessageIter subiter = iter->recurse();
    std::unique_ptr<Value> key;
    std::unique_ptr<Value> value;
    bool r = DBusMessageIterToValue(&subiter, &key);
    if (!r || key->GetType() != Value::TYPE_STRING) {
      return false;
    }
    subiter++;
    r = DBusMessageIterToValue(&subiter, &value);
    if (!r) {
      return false;
    }
    std::string keystr;
    key->GetAsString(&keystr);
    dv->Set(keystr, std::move(value));
    (*iter)++;
  }
  *result = std::move(dv);
  return true;
}

bool DBusMessageIterToValue(DBus::MessageIter* iter,
                            std::unique_ptr<Value>* result) {
  if (iter->at_end()) {
    *result = nullptr;
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

bool debugd::DBusMessageToValue(const DBus::Message& message,
                                std::unique_ptr<Value>* result) {
  DBus::MessageIter r = message.reader();
  return DBusMessageIterToArrayValue(&r, result);
}

bool debugd::DBusPropertyMapToValue(
    const std::map<std::string, DBus::Variant>& properties,
    std::unique_ptr<Value>* result) {
  auto dv = base::MakeUnique<DictionaryValue>();
  for (const auto& property : properties) {
    std::unique_ptr<Value> v;
    // make a copy so we can take a reference to the MessageIter
    DBus::MessageIter reader = property.second.reader();
    if (!DBusMessageIterToValue(&reader, &v)) {
      return false;
    }
    dv->Set(property.first, std::move(v));
  }
  *result = std::move(dv);
  return true;
}
