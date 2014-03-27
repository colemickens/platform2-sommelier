// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DBUS_MANAGER_H_
#define BUFFET_DBUS_MANAGER_H_

#include <string>

#include <base/memory/scoped_ptr.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

namespace buffet {

// Class that manages dbus interactions in buffet.
class DBusManager {
 public:
  DBusManager();
  virtual ~DBusManager();

  void Init();
  void Finalize();

  // Get an object owned by the ::dbus::Bus object.  This object
  // has methods to export DBus facing methods.
  ::dbus::ExportedObject* GetExportedObject(
      const std::string& object_path);

  // Exports |method_name| on |exported_object| and uses |member|
  // to handle calls.
  void ExportDBusMethod(
      ::dbus::ExportedObject* exported_object,
      const std::string& interface_name,
      const std::string& method_name,
      base::Callback<scoped_ptr<::dbus::Response>(
          ::dbus::MethodCall*)> handler);

 private:
  // Connects to the D-Bus system bus and exports methods.
  void InitDBus();
  void ShutDownDBus();

  // Callbacks for handling D-Bus signals and method calls.
  scoped_ptr<::dbus::Response> HandleTestMethod(
      ::dbus::MethodCall* method_call);

  scoped_refptr<::dbus::Bus> bus_;

  DISALLOW_COPY_AND_ASSIGN(DBusManager);
};

}  // namespace buffet

#endif  // BUFFET_DBUS_MANAGER_H_
