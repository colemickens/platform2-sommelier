// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <dbus-c++/dbus.h>

#include "shill/accessor_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/key_value_store.h"
#include "shill/property_store.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::Owned;
using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char DBusAdaptor::kByteArraysSig[] = "aay";
// static
const char DBusAdaptor::kPathsSig[] = "ao";
// static
const char DBusAdaptor::kStringmapSig[] = "a{ss}";
// static
const char DBusAdaptor::kStringmapsSig[] = "aa{ss}";
// static
const char DBusAdaptor::kStringsSig[] = "as";

DBusAdaptor::DBusAdaptor(DBus::Connection* conn, const string &object_path)
    : DBus::ObjectAdaptor(*conn, object_path) {
  SLOG(DBus, 2) << "DBusAdaptor: " << object_path;
}

DBusAdaptor::~DBusAdaptor() {}

// static
bool DBusAdaptor::SetProperty(PropertyStore *store,
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
    SLOG(DBus, 1) << " can't yet handle setting type " << value.signature();
    e.Populate(Error::kInternalError);
  } else if (DBusAdaptor::IsStrings(value.signature()))
    store->SetStringsProperty(name, value.operator vector<string>(), &e);
  else if (DBusAdaptor::IsUint16(value.signature()))
    store->SetUint16Property(name, value.reader().get_uint16(), &e);
  else if (DBusAdaptor::IsUint32(value.signature()))
    store->SetUint32Property(name, value.reader().get_uint32(), &e);
  else if (DBusAdaptor::IsKeyValueStore(value.signature())) {
    SLOG(DBus, 1) << " can't yet handle setting type " << value.signature();
    e.Populate(Error::kInternalError);
  } else {
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
  Error e;
  {
    ReadablePropertyConstIterator<bool> it = store.GetBoolPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = BoolToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<int16> it = store.GetInt16PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = Int16ToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<int32> it = store.GetInt32PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = Int32ToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<KeyValueStore> it =
        store.GetKeyValueStorePropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = KeyValueStoreToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<RpcIdentifiers> it =
        store.GetRpcIdentifiersPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      Strings rpc_identifiers_as_strings  = it.Value(&e);
      vector < ::DBus::Path> rpc_identifiers_as_paths;
      for (Strings::const_iterator in = rpc_identifiers_as_strings.begin();
           in != rpc_identifiers_as_strings.end();
           ++in) {
        rpc_identifiers_as_paths.push_back(*in);
      }
      (*out)[it.Key()] = PathsToVariant(rpc_identifiers_as_paths);
    }
  }
  {
    ReadablePropertyConstIterator<string> it = store.GetStringPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = StringToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<Stringmap> it =
        store.GetStringmapPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()]= StringmapToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<Stringmaps> it =
        store.GetStringmapsPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()]= StringmapsToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<Strings> it =
        store.GetStringsPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = StringsToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<uint8> it = store.GetUint8PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = ByteToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<uint16> it = store.GetUint16PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = Uint16ToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<uint32> it = store.GetUint32PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance())
      (*out)[it.Key()] = Uint32ToVariant(it.Value(&e));
  }
  {
    ReadablePropertyConstIterator<RpcIdentifier> it =
        store.GetRpcIdentifierPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = PathToVariant(it.Value(&e));
    }
  }
  return true;
}

// static
bool DBusAdaptor::ClearProperty(PropertyStore *store,
                                const string &name,
                                ::DBus::Error *error) {
  Error e;
  store->ClearProperty(name, &e);

  if (error != NULL) {
    e.ToDBusError(error);
  }

  return e.IsSuccess();
}

// static
void DBusAdaptor::ArgsToKeyValueStore(
    const map<string, ::DBus::Variant> &args,
    KeyValueStore *out,
    Error *error) {  // TODO(quiche): Should be ::DBus::Error?
  for (map<string, ::DBus::Variant>::const_iterator it = args.begin();
       it != args.end();
       ++it) {
    string key = it->first;
    DBus::type<string> string_type;
    DBus::type<bool> bool_type;

    if (it->second.signature() == string_type.sig()) {
      SLOG(DBus, 5) << "Got string property " << key;
      out->SetString(key, it->second.reader().get_string());
    } else if (it->second.signature() == bool_type.sig()) {
      SLOG(DBus, 5) << "Got bool property " << key;
      out->SetBool(key, it->second.reader().get_bool());
    } else {
      Error::PopulateAndLog(error, Error::kInternalError,
                            "unsupported type for property " + key);
      return;  // Skip remaining args after error.
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
::DBus::Variant DBusAdaptor::ByteArraysToVariant(const ByteArrays &value) {
  ::DBus::MessageIter writer;
  ::DBus::Variant v;


  // We have to use a local because the operator<< needs a reference
  // to work on (the lhs) but writer() returns by-value. C++ prohibits
  // initializing non-const references from a temporary.
  // So:
  //    v.writer() << value;
  // would NOT automagically promote the returned value of v.writer() to
  // a non-const reference (if you think about it, that's almost always not what
  // you'd want. see: http://gcc.gnu.org/ml/gcc-help/2006-04/msg00075.html).
  //
  // One could consider changing writer() to return a reference, but then it
  // changes writer() semantics as it can not be a const reference. writer()
  // currently doesn't modify the original object on which it's called.
  writer = v.writer();
  writer << value;
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
::DBus::Variant DBusAdaptor::PathsToVariant(
    const vector< ::DBus::Path> &value) {
  ::DBus::MessageIter writer;
  ::DBus::Variant v;

  // See note above on why we need to use a local.
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
::DBus::Variant DBusAdaptor::KeyValueStoreToVariant(
    const KeyValueStore &value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  map<string, ::DBus::Variant> props;
  {
    map<string, string>::const_iterator it;
    for (it = value.string_properties().begin();
         it != value.string_properties().end();
         ++it) {
      ::DBus::Variant vv;
      ::DBus::MessageIter writer = vv.writer();
      writer.append_string(it->second.c_str());
      props[it->first] = vv;
    }
  }
  {
    map<string, bool>::const_iterator it;
    for (it = value.bool_properties().begin();
         it != value.bool_properties().end();
         ++it) {
      ::DBus::Variant vv;
      ::DBus::MessageIter writer = vv.writer();
      writer.append_bool(it->second);
      props[it->first] = vv;
    }
  }
  {
    map<string, uint32>::const_iterator it;
    for (it = value.uint_properties().begin();
         it != value.uint_properties().end();
         ++it) {
      ::DBus::Variant vv;
      ::DBus::MessageIter writer = vv.writer();
      writer.append_uint32(it->second);
      props[it->first] = vv;
    }
  }
  writer << props;
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
bool DBusAdaptor::IsByteArrays(::DBus::Signature signature) {
  return signature == DBusAdaptor::kByteArraysSig;
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
bool DBusAdaptor::IsPaths(::DBus::Signature signature) {
  return signature == DBusAdaptor::kPathsSig;
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

// static
bool DBusAdaptor::IsKeyValueStore(::DBus::Signature signature) {
  return signature == ::DBus::type<map<string, ::DBus::Variant> >::sig();
}

void DBusAdaptor::DeferReply(const DBus::Tag *tag) {
  return_later(tag);
}

void DBusAdaptor::ReplyNow(const DBus::Tag *tag) {
  Continuation *cont = find_continuation(tag);
  CHECK(cont);
  return_now(cont);
}

void DBusAdaptor::ReplyNowWithError(const DBus::Tag *tag,
                                    const DBus::Error &error) {
  Continuation *cont = find_continuation(tag);
  CHECK(cont);
  return_error(cont, error);
}

ResultCallback DBusAdaptor::GetMethodReplyCallback(
    const DBus::Tag *tag) {
  return Bind(&DBusAdaptor::MethodReplyCallback, AsWeakPtr(), Owned(tag));
}

void DBusAdaptor::ReturnResultOrDefer(const DBus::Tag *tag,
                                      const Error &error,
                                      DBus::Error *dberror) {
  if (error.IsOngoing()) {
    DeferReply(tag);
  } else if (error.IsFailure()) {
    error.ToDBusError(dberror);
  }
}

void DBusAdaptor::MethodReplyCallback(const DBus::Tag *tag,
                                      const Error &error) {
  if (error.IsFailure()) {
    DBus::Error dberror;
    error.ToDBusError(&dberror);
    ReplyNowWithError(tag, dberror);
  } else {
    ReplyNow(tag);
  }
}

}  // namespace shill
