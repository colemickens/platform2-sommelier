// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_ADAPTOR_H_
#define SHILL_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "shill/accessor_interface.h"
#include "shill/adaptor_interfaces.h"
#include "shill/error.h"

namespace shill {

#define SHILL_INTERFACE "org.chromium.flimflam"
#define SHILL_PATH "/org/chromium/flimflam"

class KeyValueStore;
class PropertyStore;

// Superclass for all DBus-backed Adaptor objects
class DBusAdaptor : public DBus::ObjectAdaptor,
                    public DBus::IntrospectableAdaptor {
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
  static ::DBus::Variant PathArrayToVariant(
      const std::vector< ::DBus::Path> &value);
  static ::DBus::Variant StringToVariant(const std::string &value);
  static ::DBus::Variant StringmapToVariant(const Stringmap &value);
  static ::DBus::Variant StringmapsToVariant(const Stringmaps &value);
  static ::DBus::Variant StringsToVariant(const Strings &value);
  static ::DBus::Variant Uint16ToVariant(uint16 value);
  static ::DBus::Variant Uint32ToVariant(uint32 value);

  static bool IsBool(::DBus::Signature signature);
  static bool IsByte(::DBus::Signature signature);
  static bool IsByteArrays(::DBus::Signature signature);
  static bool IsInt16(::DBus::Signature signature);
  static bool IsInt32(::DBus::Signature signature);
  static bool IsPath(::DBus::Signature signature);
  static bool IsPathArray(::DBus::Signature signature);
  static bool IsString(::DBus::Signature signature);
  static bool IsStringmap(::DBus::Signature signature);
  static bool IsStringmaps(::DBus::Signature signature);
  static bool IsStrings(::DBus::Signature signature);
  static bool IsUint16(::DBus::Signature signature);
  static bool IsUint32(::DBus::Signature signature);

 protected:
  class Returner : public DBus::Tag,
                   public ReturnerInterface {
   public:
    // Creates a new returner instance associated with |adaptor|.
    static Returner *Create(DBusAdaptor *adaptor);

    // Used by the adaptor to initiate or delay the return, depending on the
    // state of the returner. A call to this method should be the last statement
    // in the adaptor method. If none of the interface Return* methods has been
    // called yet, DelayOrReturn exits to the dbus-c++ message handler by
    // throwing an exception. Otherwise, it initializes |error|, completes the
    // RPC call right away and destroys |this|.
    void DelayOrReturn(DBus::Error *error);

    // Inherited from ReturnerInterface. These methods complete the RPC call
    // right away and destroy the object if DelayOrReturn has been called
    // already. Otherwise, they allow DelayOrReturn to complete the call.
    virtual void Return();
    virtual void ReturnError(const Error &error);

   private:
    // The returner transitions through the following states:
    //
    // Initialized -> [Delayed|Returned] -> Destroyed.
    enum State {
      kStateInitialized,  // No *Return* methods called yet.
      kStateDelayed,  // DelayOrReturn called, Return* not.
      kStateReturned,  // Return* called, DelayOrReturn not.
      kStateDestroyed  // Return complete, returner destroyed.
    };

    explicit Returner(DBusAdaptor *adaptor);

    // Destruction happens through the *Return* methods.
    virtual ~Returner();

    DBusAdaptor *adaptor_;
    Error error_;
    State state_;

    DISALLOW_COPY_AND_ASSIGN(Returner);
  };

 private:
  static const char kByteArraysSig[];
  static const char kPathArraySig[];
  static const char kStringmapSig[];
  static const char kStringmapsSig[];
  static const char kStringsSig[];

  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_DBUS_ADAPTOR_H_
