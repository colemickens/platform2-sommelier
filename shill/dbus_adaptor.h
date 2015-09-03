//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DBUS_ADAPTOR_H_
#define SHILL_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <dbus-c++/dbus.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

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
  static const char kNullPath[];

  DBusAdaptor(DBus::Connection* conn, const std::string& object_path);
  ~DBusAdaptor() override;

  // Set the property with |name| through |store|. Returns true if and
  // only if the property was changed. Updates |error| if a) an error
  // was encountered, and b) |error| is non-NULL. Otherwise, |error| is
  // unchanged.
  static bool SetProperty(PropertyStore* store,
                          const std::string& name,
                          const ::DBus::Variant& value,
                          ::DBus::Error* error);
  static bool GetProperties(const PropertyStore& store,
                            std::map<std::string, ::DBus::Variant>* out,
                            ::DBus::Error* error);
  // Look for a property with |name| in |store|. If found, reset the
  // property to its "factory" value. If the property can not be
  // found, or if it can not be cleared (e.g., because it is
  // read-only), set |error| accordingly.
  //
  // Returns true if the property was found and cleared; returns false
  // otherwise.
  static bool ClearProperty(PropertyStore* store,
                            const std::string& name,
                            ::DBus::Error* error);
  static void ArgsToKeyValueStore(
      const std::map<std::string, ::DBus::Variant>& args,
      KeyValueStore* out,
      Error* error);

  static ::DBus::Variant BoolToVariant(bool value);
  static ::DBus::Variant ByteArraysToVariant(const ByteArrays& value);
  static ::DBus::Variant ByteToVariant(uint8_t value);
  static ::DBus::Variant Int16ToVariant(int16_t value);
  static ::DBus::Variant Int32ToVariant(int32_t value);
  static ::DBus::Variant KeyValueStoreToVariant(const KeyValueStore& value);
  static ::DBus::Variant PathToVariant(const ::DBus::Path& value);
  static ::DBus::Variant PathsToVariant(const std::vector<::DBus::Path>& value);
  static ::DBus::Variant StringToVariant(const std::string& value);
  static ::DBus::Variant StringmapToVariant(const Stringmap& value);
  static ::DBus::Variant StringmapsToVariant(const Stringmaps& value);
  static ::DBus::Variant StringsToVariant(const Strings& value);
  static ::DBus::Variant Uint16ToVariant(uint16_t value);
  static ::DBus::Variant Uint16sToVariant(const Uint16s& value);
  static ::DBus::Variant Uint32ToVariant(uint32_t value);
  static ::DBus::Variant Uint64ToVariant(uint64_t value);

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
  static bool IsUint16s(::DBus::Signature signature);
  static bool IsUint32(::DBus::Signature signature);
  static bool IsUint64(::DBus::Signature signature);
  static bool IsKeyValueStore(::DBus::Signature signature);

 protected:
  FRIEND_TEST(DBusAdaptorTest, SanitizePathElement);

  ResultCallback GetMethodReplyCallback(const DBus::Tag* tag);
  // It would be nice if these two methods could be templated.  Unfortunately,
  // attempts to do so will trigger some fairly esoteric warnings from the
  // base library.
  ResultStringCallback GetStringMethodReplyCallback(const DBus::Tag* tag);
  ResultBoolCallback GetBoolMethodReplyCallback(const DBus::Tag* tag);

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
  // void XXXXDBusAdaptor::SomeMethod(<args...>, DBus::Error& error) {
  //   Error e(Error::kOperationInitiated);
  //   DBus::Tag* tag = new DBus::Tag();
  //   xxxx_->SomeMethod(<args...>, &e, GetMethodReplyCallback(tag));
  //   ReturnResultOrDefer(tag, e, &error);
  // }
  //
  void ReturnResultOrDefer(const DBus::Tag* tag,
                           const Error& error,
                           DBus::Error* dberror);

  // Returns an object path fragment that conforms to D-Bus specifications.
  static std::string SanitizePathElement(const std::string& object_path);

 private:
  void MethodReplyCallback(const DBus::Tag* tag, const Error& error);
  void StringMethodReplyCallback(const DBus::Tag* tag, const Error& error,
                                 const std::string& returned);
  void BoolMethodReplyCallback(const DBus::Tag* tag, const Error& error,
                               bool returned);
  template<typename T>
  void TypedMethodReplyCallback(const DBus::Tag* tag, const Error& error,
                                const T& returned);
  void DeferReply(const DBus::Tag* tag);
  void ReplyNow(const DBus::Tag* tag);
  template <typename T>
  void TypedReplyNow(const DBus::Tag* tag, const T& value);
  void ReplyNowWithError(const DBus::Tag* tag, const DBus::Error& error);

  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_DBUS_ADAPTOR_H_
