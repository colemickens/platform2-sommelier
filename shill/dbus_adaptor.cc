// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <dbus-c++/dbus.h>

#include "shill/accessor_interface.h"
#include "shill/error.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/property_store.h"

using base::Bind;
using base::Owned;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(DBusAdaptor* d) {
  if (d == nullptr)
    return "(dbus_adaptor)";
  return d->path();
}
}

// public static
const char DBusAdaptor::kNullPath[] = "/";

DBusAdaptor::DBusAdaptor(DBus::Connection* conn, const string& object_path)
    : DBus::ObjectAdaptor(*conn, object_path) {
  SLOG(this, 2) << "DBusAdaptor: " << object_path;
}

DBusAdaptor::~DBusAdaptor() {}

// static
bool DBusAdaptor::SetProperty(PropertyStore* store,
                              const string& name,
                              const ::DBus::Variant& value,
                              ::DBus::Error* error) {
  Error e;
  bool ret;

  if (DBusAdaptor::IsBool(value.signature())) {
    ret = store->SetBoolProperty(name, value.reader().get_bool(), &e);
  } else if (DBusAdaptor::IsByte(value.signature())) {
    ret = store->SetUint8Property(name, value.reader().get_byte(), &e);
  } else if (DBusAdaptor::IsInt16(value.signature())) {
    ret = store->SetInt16Property(name, value.reader().get_int16(), &e);
  } else if (DBusAdaptor::IsInt32(value.signature())) {
    ret = store->SetInt32Property(name, value.reader().get_int32(), &e);
  } else if (DBusAdaptor::IsPath(value.signature())) {
    ret = store->SetStringProperty(name, value.reader().get_path(), &e);
  } else if (DBusAdaptor::IsString(value.signature())) {
    ret = store->SetStringProperty(name, value.reader().get_string(), &e);
  } else if (DBusAdaptor::IsStringmap(value.signature())) {
    ret = store->SetStringmapProperty(name,
                                      value.operator map<string, string>(),
                                      &e);
  } else if (DBusAdaptor::IsStringmaps(value.signature())) {
    SLOG(nullptr, 1) << " can't yet handle setting type " << value.signature();
    ret = false;
    e.Populate(Error::kInternalError);
  } else if (DBusAdaptor::IsStrings(value.signature())) {
    ret = store->SetStringsProperty(name, value.operator vector<string>(), &e);
  } else if (DBusAdaptor::IsUint16(value.signature())) {
    ret = store->SetUint16Property(name, value.reader().get_uint16(), &e);
  } else if (DBusAdaptor::IsUint16s(value.signature())) {
    ret =
        store->SetUint16sProperty(name, value.operator vector<uint16_t>(), &e);
  } else if (DBusAdaptor::IsUint32(value.signature())) {
    ret = store->SetUint32Property(name, value.reader().get_uint32(), &e);
  } else if (DBusAdaptor::IsUint64(value.signature())) {
    ret = store->SetUint64Property(name, value.reader().get_uint64(), &e);
  } else if (DBusAdaptor::IsKeyValueStore(value.signature())) {
    KeyValueStore key_value_store;
    auto dict = value.operator map<string, DBus::Variant>();
    DBusAdaptor::ArgsToKeyValueStore(dict, &key_value_store, &e);
    if (e.IsSuccess()) {
      ret = store->SetKeyValueStoreProperty(name, key_value_store, &e);
    } else {
      ret = false;
    }
  } else {
    NOTREACHED() << " unknown type: " << value.signature();
    ret = false;
    e.Populate(Error::kInternalError);
  }

  if (error != nullptr) {
    e.ToDBusError(error);
  }

  return ret;
}

// static
bool DBusAdaptor::GetProperties(const PropertyStore& store,
                                map<string, ::DBus::Variant>* out,
                                ::DBus::Error* /*error*/) {
  {
    ReadablePropertyConstIterator<bool> it = store.GetBoolPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = BoolToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<int16_t> it = store.GetInt16PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = Int16ToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<int32_t> it = store.GetInt32PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = Int32ToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<KeyValueStore> it =
        store.GetKeyValueStorePropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = KeyValueStoreToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<RpcIdentifiers> it =
        store.GetRpcIdentifiersPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      vector < ::DBus::Path> rpc_identifiers_as_paths;
      for (const auto& path : it.value()) {
        rpc_identifiers_as_paths.push_back(path);
      }
      (*out)[it.Key()] = PathsToVariant(rpc_identifiers_as_paths);
    }
  }
  {
    ReadablePropertyConstIterator<string> it = store.GetStringPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = StringToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<Stringmap> it =
        store.GetStringmapPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()]= StringmapToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<Stringmaps> it =
        store.GetStringmapsPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()]= StringmapsToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<Strings> it =
        store.GetStringsPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = StringsToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<uint8_t> it = store.GetUint8PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = ByteToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<uint16_t> it =
        store.GetUint16PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = Uint16ToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<Uint16s> it =
        store.GetUint16sPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = Uint16sToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<uint32_t> it =
        store.GetUint32PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = Uint32ToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<uint64_t> it =
        store.GetUint64PropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = Uint64ToVariant(it.value());
    }
  }
  {
    ReadablePropertyConstIterator<RpcIdentifier> it =
        store.GetRpcIdentifierPropertiesIter();
    for ( ; !it.AtEnd(); it.Advance()) {
      (*out)[it.Key()] = PathToVariant(it.value());
    }
  }
  return true;
}

// static
bool DBusAdaptor::ClearProperty(PropertyStore* store,
                                const string& name,
                                ::DBus::Error* error) {
  Error e;
  store->ClearProperty(name, &e);

  if (error != nullptr) {
    e.ToDBusError(error);
  }

  return e.IsSuccess();
}

// static
void DBusAdaptor::ArgsToKeyValueStore(
    const map<string, ::DBus::Variant>& args,
    KeyValueStore* out,
    Error* error) {  // TODO(quiche): Should be ::DBus::Error?
  for (const auto& key_value_pair : args) {
    string key = key_value_pair.first;
    DBus::type<string> string_type;
    DBus::type<bool> bool_type;
    DBus::type<int32_t> int32_type;

    if (key_value_pair.second.signature() == bool_type.sig()) {
      SLOG(nullptr, 5) << "Got bool property " << key;
      out->SetBool(key, key_value_pair.second.reader().get_bool());
    } else if (key_value_pair.second.signature() == int32_type.sig()) {
      SLOG(nullptr, 5) << "Got int32_t property " << key;
      out->SetInt(key, key_value_pair.second.reader().get_int32());
    } else if (key_value_pair.second.signature() == string_type.sig()) {
      SLOG(nullptr, 5) << "Got string property " << key;
      out->SetString(key, key_value_pair.second.reader().get_string());
    } else if (DBusAdaptor::IsKeyValueStore(
                   key_value_pair.second.signature())) {
      // Unwrap a recursive KeyValueStore object.
      KeyValueStore store;
      Error convert_error;
      DBusAdaptor::ArgsToKeyValueStore(
          key_value_pair.second.operator map<string, ::DBus::Variant>(),
          &store,
          &convert_error);
      if (convert_error.IsSuccess()) {
          out->SetKeyValueStore(key, store);
      } else {
          Error::PopulateAndLog(FROM_HERE, error, convert_error.type(),
                                convert_error.message() + " in sub-key " + key);
          return;  // Skip remaining args after error.
      }
    } else if (DBusAdaptor::IsStrings(key_value_pair.second.signature())) {
      SLOG(nullptr, 5) << "Got strings property " << key;
      out->SetStrings(key, key_value_pair.second.operator vector<string>());
    } else if (DBusAdaptor::IsStringmap(key_value_pair.second.signature())) {
      SLOG(nullptr, 5) << "Got stringmap property " << key;
      out->SetStringmap(
          key, key_value_pair.second.operator map<string, string>());
    } else {
      Error::PopulateAndLog(FROM_HERE, error, Error::kInternalError,
                            "unsupported type for property " + key);
      return;  // Skip remaining args after error.
    }
  }
}

// static
string DBusAdaptor::SanitizePathElement(const string& object_path) {
  string sanitized_path(object_path);
  size_t length = sanitized_path.length();

  for (size_t i = 0; i < length; ++i) {
    char c = sanitized_path[i];
    // The D-Bus specification
    // (http://dbus.freedesktop.org/doc/dbus-specification.html) states:
    // Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_"
    if (!(c >= 'A' && c <= 'Z') &&
        !(c >= 'a' && c <= 'z') &&
        !(c >= '0' && c <= '9') &&
        c != '_') {
      sanitized_path[i] = '_';
    }
  }

  return sanitized_path;
}

// static
::DBus::Variant DBusAdaptor::BoolToVariant(bool value) {
  ::DBus::Variant v;
  v.writer().append_bool(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::ByteArraysToVariant(const ByteArrays& value) {
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
::DBus::Variant DBusAdaptor::ByteToVariant(uint8_t value) {
  ::DBus::Variant v;
  v.writer().append_byte(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::Int16ToVariant(int16_t value) {
  ::DBus::Variant v;
  v.writer().append_int16(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::Int32ToVariant(int32_t value) {
  ::DBus::Variant v;
  v.writer().append_int32(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::PathToVariant(const ::DBus::Path& value) {
  ::DBus::Variant v;
  v.writer().append_path(value.c_str());
  return v;
}

// static
::DBus::Variant DBusAdaptor::PathsToVariant(const vector<::DBus::Path>& value) {
  ::DBus::MessageIter writer;
  ::DBus::Variant v;

  // See note above on why we need to use a local.
  writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringToVariant(const string& value) {
  ::DBus::Variant v;
  v.writer().append_string(value.c_str());
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringmapToVariant(const Stringmap& value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringmapsToVariant(const Stringmaps& value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringsToVariant(const Strings& value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::KeyValueStoreToVariant(
    const KeyValueStore& value) {
  DBusPropertiesMap props;
  DBusProperties::ConvertKeyValueStoreToMap(value, &props);
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << props;
  return v;
}

// static
::DBus::Variant DBusAdaptor::Uint16ToVariant(uint16_t value) {
  ::DBus::Variant v;
  v.writer().append_uint16(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::Uint16sToVariant(const Uint16s& value) {
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << value;
  return v;
}

// static
::DBus::Variant DBusAdaptor::Uint32ToVariant(uint32_t value) {
  ::DBus::Variant v;
  v.writer().append_uint32(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::Uint64ToVariant(uint64_t value) {
  ::DBus::Variant v;
  v.writer().append_uint64(value);
  return v;
}

// static
bool DBusAdaptor::IsBool(::DBus::Signature signature) {
  return signature == ::DBus::type<bool>::sig();
}

// static
bool DBusAdaptor::IsByte(::DBus::Signature signature) {
  return signature == ::DBus::type<uint8_t>::sig();
}

// static
bool DBusAdaptor::IsByteArrays(::DBus::Signature signature) {
  return signature == ::DBus::type<ByteArrays>::sig();
}

// static
bool DBusAdaptor::IsInt16(::DBus::Signature signature) {
  return signature == ::DBus::type<int16_t>::sig();
}

// static
bool DBusAdaptor::IsInt32(::DBus::Signature signature) {
  return signature == ::DBus::type<int32_t>::sig();
}

// static
bool DBusAdaptor::IsPath(::DBus::Signature signature) {
  return signature == ::DBus::type<::DBus::Path>::sig();
}

// static
bool DBusAdaptor::IsPaths(::DBus::Signature signature) {
  return signature == ::DBus::type<vector<::DBus::Path>>::sig();
}

// static
bool DBusAdaptor::IsString(::DBus::Signature signature) {
  return signature == ::DBus::type<string>::sig();
}

// static
bool DBusAdaptor::IsStringmap(::DBus::Signature signature) {
  return signature == ::DBus::type<Stringmap>::sig();
}

// static
bool DBusAdaptor::IsStringmaps(::DBus::Signature signature) {
  return signature == ::DBus::type<Stringmaps>::sig();
}

// static
bool DBusAdaptor::IsStrings(::DBus::Signature signature) {
  return signature == ::DBus::type<Strings>::sig();
}

// static
bool DBusAdaptor::IsUint16(::DBus::Signature signature) {
  return signature == ::DBus::type<uint16_t>::sig();
}

// static
bool DBusAdaptor::IsUint16s(::DBus::Signature signature) {
  return signature == ::DBus::type<Uint16s>::sig();
}

// static
bool DBusAdaptor::IsUint32(::DBus::Signature signature) {
  return signature == ::DBus::type<uint32_t>::sig();
}

// static
bool DBusAdaptor::IsUint64(::DBus::Signature signature) {
  return signature == ::DBus::type<uint64_t>::sig();
}

// static
bool DBusAdaptor::IsKeyValueStore(::DBus::Signature signature) {
  return signature == ::DBus::type<map<string, ::DBus::Variant>>::sig();
}

void DBusAdaptor::DeferReply(const DBus::Tag* tag) {
  return_later(tag);
}

void DBusAdaptor::ReplyNow(const DBus::Tag* tag) {
  Continuation* cont = find_continuation(tag);
  CHECK(cont) << "Failed to find continuation.";
  return_now(cont);
}

template <typename T>
void DBusAdaptor::TypedReplyNow(const DBus::Tag* tag, const T& value) {
  Continuation* cont = find_continuation(tag);
  CHECK(cont) << "Failed to find continuation.";
  cont->writer() << value;
  return_now(cont);
}

void DBusAdaptor::ReplyNowWithError(const DBus::Tag* tag,
                                    const DBus::Error& error) {
  Continuation* cont = find_continuation(tag);
  CHECK(cont) << "Failed to find continuation.";
  SLOG(this, 1) << "Returning error: (" << error.name() << ": "
                << error.message() << ")";
  return_error(cont, error);
}

ResultCallback DBusAdaptor::GetMethodReplyCallback(
    const DBus::Tag* tag) {
  return Bind(&DBusAdaptor::MethodReplyCallback, AsWeakPtr(), Owned(tag));
}

ResultStringCallback DBusAdaptor::GetStringMethodReplyCallback(
    const DBus::Tag* tag) {
  return Bind(&DBusAdaptor::StringMethodReplyCallback, AsWeakPtr(), Owned(tag));
}

ResultBoolCallback DBusAdaptor::GetBoolMethodReplyCallback(
    const DBus::Tag* tag) {
  return Bind(&DBusAdaptor::BoolMethodReplyCallback, AsWeakPtr(), Owned(tag));
}

template<typename T>
void DBusAdaptor::TypedMethodReplyCallback(const DBus::Tag* tag,
                                           const Error& error,
                                           const T& returned) {
  if (error.IsFailure()) {
    DBus::Error dberror;
    error.ToDBusError(&dberror);
    ReplyNowWithError(tag, dberror);
  } else {
    TypedReplyNow(tag, returned);
  }
}

void DBusAdaptor::ReturnResultOrDefer(const DBus::Tag* tag,
                                      const Error& error,
                                      DBus::Error* dberror) {
  if (error.IsOngoing()) {
    DeferReply(tag);
  } else if (error.IsFailure()) {
    error.ToDBusError(dberror);
  }
}

void DBusAdaptor::MethodReplyCallback(const DBus::Tag* tag,
                                      const Error& error) {
  if (error.IsFailure()) {
    DBus::Error dberror;
    error.ToDBusError(&dberror);
    ReplyNowWithError(tag, dberror);
  } else {
    ReplyNow(tag);
  }
}

void DBusAdaptor::StringMethodReplyCallback(const DBus::Tag* tag,
                                            const Error& error,
                                            const string& returned) {
  TypedMethodReplyCallback(tag, error, returned);
}

void DBusAdaptor::BoolMethodReplyCallback(const DBus::Tag* tag,
                                          const Error& error,
                                          bool returned) {
  TypedMethodReplyCallback(tag, error, returned);
}

}  // namespace shill
