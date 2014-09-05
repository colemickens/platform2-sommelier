// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASY_UNLOCK_DBUS_ADAPTOR_H_
#define EASY_UNLOCK_DBUS_ADAPTOR_H_

#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

namespace dbus {
class ExportedObject;
class MethodCall;
class Response;
}  // namespace dbus

namespace easy_unlock {
class Service;
}  // namespace easy_unlock

namespace easy_unlock {

// DBus adaptor for EasyUnlock dbus service.
class DBusAdaptor {
 public:
  explicit DBusAdaptor(easy_unlock::Service* service);
  ~DBusAdaptor();

  // Registers handlers for EasyUnlock service method calls.
  void ExportDBusMethods(dbus::ExportedObject* object);

 private:
  // Handlere for Introspect DBus method.
  scoped_ptr<dbus::Response> Introspect(dbus::MethodCall* call);
  // Handlers for DBus method calls exported in |ExportDBusMethods|.
  // See service_impl.h in easy-unlock-crypto repo for more info on specific
  // methods.
  scoped_ptr<dbus::Response> GenerateEcP256KeyPair(dbus::MethodCall* call);
  scoped_ptr<dbus::Response> PerformECDHKeyAgreement(dbus::MethodCall* call);
  scoped_ptr<dbus::Response> CreateSecureMessage(dbus::MethodCall* call);
  scoped_ptr<dbus::Response> UnwrapSecureMessage(dbus::MethodCall* call);

  typedef scoped_ptr<dbus::Response> (
      DBusAdaptor::*SyncDBusMethodCallMemberFunction)(
          dbus::MethodCall*);
  void ExportSyncDBusMethod(dbus::ExportedObject* object,
                            const std::string& method_name,
                            SyncDBusMethodCallMemberFunction member);

  // The EasyUnlock service implementation to which DBus method calls
  // are forwarded.
  easy_unlock::Service* const service_impl_;

  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

}  // namespace easy_unlock

#endif  // EASY_UNLOCK_DBUS_ADAPTOR_H_
