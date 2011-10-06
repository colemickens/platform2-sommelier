// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <dbus-c++/dbus.h>

#include "shill/accessor_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/key_value_store.h"
#include "shill/property_store.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char DBusAdaptor::kPathArraySig[] = "ao";
// static
const char DBusAdaptor::kStringmapSig[] = "a{ss}";
// static
const char DBusAdaptor::kStringmapsSig[] = "aa{ss}";
// static
const char DBusAdaptor::kStringsSig[] = "as";

DBusAdaptor::DBusAdaptor(DBus::Connection* conn, const string &object_path)
    : DBus::ObjectAdaptor(*conn, object_path) {
  VLOG(2) << "DBusAdaptor: " << object_path;
}

DBusAdaptor::~DBusAdaptor() {}

// static
bool DBusAdaptor::DispatchOnType(PropertyStore *store,
                                 const string &name,
                                 const ::DBus::Variant &value,
                                 ::DBus::Error *error) {
  Error e;

  if (DBusAdaptor::IsBool(value.signature()))
    store->SetBoolProperty(name, value.reader().get_bool(), &e);
  else if (DBusAdaptor::IsByte(value.signature()))
    store->SetUint8Property(name, value.reader().get_byte(), &e);
  else if (DBusAdaptor::IsInt16(value.signature()))
    store->SetInt16Property(name, value.reader().get_int16(), &e);
  else if (DBusAdaptor::IsInt32(value.signature()))
    store->SetInt32Property(name, value.reader().get_int32(), &e);
  else if (DBusAdaptor::IsPath(value.signature()))
    store->SetStringProperty(name, value.reader().get_path(), &e);
  else if (DBusAdaptor::IsString(value.signature()))
    store->SetStringProperty(name, value.reader().get_string(), &e);
  else if (DBusAdaptor::IsStringmap(value.signature()))
    store->SetStringmapProperty(name,
                                value.operator map<string, string>(),
                                &e);
  else if (DBusAdaptor::IsStringmaps(value.signature())) {
    VLOG(1) << " can't yet handle setting type " << value.signature();
    e.Populate(Error::kInternalError);
  } else if (DBusAdaptor::IsStrings(value.signature()))
    store->SetStringsProperty(name, value.operator vector<string>(), &e);
  else if (DBusAdaptor::IsUint16(value.signature()))
    store->SetUint16Property(name, value.reader().get_uint16(), &e);
  else if (DBusAdaptor::IsUint32(value.signature()))
    store->SetUint32Property(name, value.reader().get_uint32(), &e);
  else {
    NOTREACHED() << " unknown type: " << value.signature();
    e.Populate(Error::kInternalError);
  }

  if (error != NULL) {
    e.ToDBusError(error);
  }

  return e.IsSuccess();
}

// static
bool DBusAdaptor::GetProperties(const PropertyStore &store,
                                map<string, ::DBus::Variant> *out,
                                ::DBus::Error */*error*/) {
  {
    PropertyConstIterator<bool> it = store.GetBoolPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = BoolToVariant(it.Value());
  }
  {
    PropertyConstIterator<int16> it = store.GetInt16PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = Int16ToVariant(it.Value());
  }
  {
    PropertyConstIterator<int32> it = store.GetInt32PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = Int32ToVariant(it.Value());
  }
  {
    PropertyConstIterator<string> it = store.GetStringPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = StringToVariant(it.Value());
  }
  {
    PropertyConstIterator<Stringmap> it = store.GetStringmapPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = StringmapToVariant(it.Value());
  }
  {
    PropertyConstIterator<Strings> it = store.GetStringsPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = StringsToVariant(it.Value());
  }
  {
    PropertyConstIterator<uint8> it = store.GetUint8PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = ByteToVariant(it.Value());
  }
  {
    PropertyConstIterator<uint16> it = store.GetUint16PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = Uint16ToVariant(it.Value());
  }
  {
    PropertyConstIterator<uint32> it = store.GetUint32PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = Uint32ToVariant(it.Value());
  }
  return true;
}

// static
void DBusAdaptor::ArgsToKeyValueStore(
    const map<string, ::DBus::Variant> &args,
    KeyValueStore *out,
    Error *error) {  // XXX should be ::DBus::Error?
  for (map<string, ::DBus::Variant>::const_iterator it = args.begin();
       it != args.end();
       ++it) {
    DBus::type<string> string_type;
    DBus::type<bool> bool_type;

    if (it->second.signature() == string_type.sig()) {
      out->SetString(it->first, it->second.reader().get_string());
    } else if (it->second.signature() == bool_type.sig()) {
      out->SetBool(it->first, it->second.reader().get_bool());
    } else {
      error->Populate(Error::kInternalError);
      return;  // skip remaining args after error
    }
  }
}

// static
::DBus::Variant DBusAdaptor::BoolToVariant(bool value) {
  ::DBus::Variant v;
  v.writer().append_bool(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::ByteToVariant(uint8 value) {
  ::DBus::Variant v;
  v.writer().append_byte(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::Int16ToVariant(int16 value) {
  ::DBus::Variant v;
  v.writer().append_int16(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::Int32ToVariant(int32 value) {
  ::DBus::Variant v;
  v.writer().append_int32(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::PathToVariant(const ::DBus::Path &value) {
  ::DBus::Variant v;
  v.writer().append_path(value.c_str());
  return v;
}

// static
::DBus::Variant DBusAdaptor::PathArrayToVariant(
    const vector< ::DBus::Path> &value) {
  ::DBus::MessageIter writer;
  ::DBus::Variant v;

  // TODO(quiche): figure out why we can't use operator<< without the
  // temporary variable.
  writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringToVariant(const string &value) {
  ::DBus::Variant v;
  v.writer().append_string(value.c_str());
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringmapToVariant(const Stringmap &value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringmapsToVariant(const Stringmaps &value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringsToVariant(const Strings &value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::StrIntPairToVariant(const StrIntPair &value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << value.string_property();
  writer << value.uint_property();
  return v;
}

// static
::DBus::Variant DBusAdaptor::Uint16ToVariant(uint16 value) {
  ::DBus::Variant v;
  v.writer().append_uint16(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::Uint32ToVariant(uint32 value) {
  ::DBus::Variant v;
  v.writer().append_uint32(value);
  return v;
}

// static
bool DBusAdaptor::IsBool(::DBus::Signature signature) {
  return signature == ::DBus::type<bool>::sig();
}

// static
bool DBusAdaptor::IsByte(::DBus::Signature signature) {
  return signature == ::DBus::type<uint8>::sig();
}

// static
bool DBusAdaptor::IsInt16(::DBus::Signature signature) {
  return signature == ::DBus::type<int16>::sig();
}

// static
bool DBusAdaptor::IsInt32(::DBus::Signature signature) {
  return signature == ::DBus::type<int32>::sig();
}

// static
bool DBusAdaptor::IsPath(::DBus::Signature signature) {
  return signature == ::DBus::type< ::DBus::Path >::sig();
}

// static
bool DBusAdaptor::IsPathArray(::DBus::Signature signature) {
  return signature == DBusAdaptor::kPathArraySig;
}

// static
bool DBusAdaptor::IsString(::DBus::Signature signature) {
  return signature == ::DBus::type<string>::sig();
}

// static
bool DBusAdaptor::IsStringmap(::DBus::Signature signature) {
  return signature == DBusAdaptor::kStringmapSig;
}

// static
bool DBusAdaptor::IsStringmaps(::DBus::Signature signature) {
  return signature == DBusAdaptor::kStringmapsSig;
}

// static
bool DBusAdaptor::IsStrings(::DBus::Signature signature) {
  return signature == DBusAdaptor::kStringsSig;
}

// static
bool DBusAdaptor::IsUint16(::DBus::Signature signature) {
  return signature == ::DBus::type<uint16>::sig();
}

// static
bool DBusAdaptor::IsUint32(::DBus::Signature signature) {
  return signature == ::DBus::type<uint32>::sig();
}

}  // namespace shill
