// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_ADAPTOR_H_
#define SHILL_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <dbus-c++/dbus.h>

#include "shill/accessor_interface.h"
#include "shill/adaptor_interfaces.h"
#include "shill/callbacks.h"
#include "shill/error.h"

namespace shill {

#define SHILL_INTERFACE "org.chromium.flimflam"
#define SHILL_PATH "/org/chromium/flimflam"

class KeyValueStore;
class PropertyStore;

// Superclass for all DBus-backed Adaptor objects
class DBusAdaptor : public DBus::ObjectAdaptor,
                    public DBus::IntrospectableAdaptor,
                    public base::SupportsWeakPtr<DBusAdaptor> {
 public:
  DBusAdaptor(DBus::Connection* conn, const std::string &object_path);
  virtual ~DBusAdaptor();

  static bool SetProperty(PropertyStore *store,
                          const std::string &name,
                          const ::DBus::Variant &value,
                          ::DBus::Error *error);
  static bool GetProperties(const PropertyStore &store,
                            std::map<std::string, ::DBus::Variant> *out,
                            ::DBus::Error *error);
  // Look for a property with |name| in |store|. If found, reset the
  // property to its "factory" value. If the property can not be
  // found, or if it can not be cleared (e.g., because it is
  // read-only), set |error| accordingly.
  //
  // Returns true if the property was found and cleared; returns false
  // otherwise.
  static bool ClearProperty(PropertyStore *store,
                            const std::string &name,
                            ::DBus::Error *error);
  static void ArgsToKeyValueStore(
      const std::map<std::string, ::DBus::Variant> &args,
      KeyValueStore *out,
      Error *error);

  static ::DBus::Variant BoolToVariant(bool value);
  static ::DBus::Variant ByteArraysToVariant(const ByteArrays &value);
  static ::DBus::Variant ByteToVariant(uint8 value);
  static ::DBus::Variant Int16ToVariant(int16 value);
  static ::DBus::Variant Int32ToVariant(int32 value);
  static ::DBus::Variant KeyValueStoreToVariant(const KeyValueStore &value);
  static ::DBus::Variant PathToVariant(const ::DBus::Path &value);
  static ::DBus::Variant PathsToVariant(
      const std::vector< ::DBus::Path> &value);
  static ::DBus::Variant StringToVariant(const std::string &value);
  static ::DBus::Variant StringmapToVariant(const Stringmap &value);
  static ::DBus::Variant StringmapsToVariant(const Stringmaps &value);
  static ::DBus::Variant StringsToVariant(const Strings &value);
  static ::DBus::Variant Uint16ToVariant(uint16 value);
  static ::DBus::Variant Uint32ToVariant(uint32 value);
  static ::DBus::Variant Uint64ToVariant(uint64 value);

  static bool IsBool(::DBus::Signature signature);
  static bool IsByte(::DBus::Signature signature);
  static bool IsByteArrays(::DBus::Signature signature);
  static bool IsInt16(::DBus::Signature signature);
  static bool IsInt32(::DBus::Signature signature);
  static bool IsPath(::DBus::Signature signature);
  static bool IsPaths(::DBus::Signature signature);
  static bool IsString(::DBus::Signature signature);
  static bool IsStringmap(::DBus::Signature signature);
  static bool IsStringmaps(::DBus::Signature signature);
  static bool IsStrings(::DBus::Signature signature);
  static bool IsUint16(::DBus::Signature signature);
  static bool IsUint32(::DBus::Signature signature);
  static bool IsUint64(::DBus::Signature signature);
  static bool IsKeyValueStore(::DBus::Signature signature);

 protected:
  ResultCallback GetMethodReplyCallback(const DBus::Tag *tag);
  // Adaptors call this method just before returning. If |error|
  // indicates that the operation has completed, with no asynchronously
  // delivered result expected, then a DBus method reply is immediately
  // sent to the client that initiated the method invocation. Otherwise,
  // the operation is ongoing, and the result will be sent to the client
  // when the operation completes at some later time.
  //
  // Adaptors should always construct an Error initialized to the value
  // Error::kOperationInitiated. A pointer to this Error is passed down
  // through the call stack. Any layer that determines that the operation
  // has completed, either because of a failure that prevents carrying it
  // out, or because it was possible to complete it without sending a request
  // to an external server, should call error.Reset() to indicate success,
  // or to some error type to reflect the kind of failure that occurred.
  // Otherwise, they should leave the Error alone.
  //
  // The general structure of an adaptor method is
  //
  // void XXXXDBusAdaptor::SomeMethod(<args...>, DBus::Error &error) {
  //   Error e(Error::kOperationInitiated);
  //   DBus::Tag *tag = new DBus::Tag();
  //   xxxx_->SomeMethod(<args...>, &e, GetMethodReplyCallback(tag));
  //   ReturnResultOrDefer(tag, e, &error);
  // }
  //
  void ReturnResultOrDefer(const DBus::Tag *tag,
                           const Error &error,
                           DBus::Error *dberror);

 private:
  static const char kByteArraysSig[];
  static const char kPathsSig[];
  static const char kStringmapSig[];
  static const char kStringmapsSig[];
  static const char kStringsSig[];

  void MethodReplyCallback(const DBus::Tag *tag, const Error &error);
  void DeferReply(const DBus::Tag *tag);
  void ReplyNow(const DBus::Tag *tag);
  void ReplyNowWithError(const DBus::Tag *tag, const DBus::Error &error);

  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_DBUS_ADAPTOR_H_
