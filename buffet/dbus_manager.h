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

// TODO(sosa): Move to chromeos/system_api once we're ready.
const char kBuffetInterface[] = "org.chromium.Buffet";
const char kBuffetServicePath[] = "/org/chromium/Buffet";
const char kBuffetServiceName[] = "org.chromium.Buffet";

// Methods exposed by buffet.
const char kTestMethod[] = "TestMethod";

class DBusManager;

// Pointer to a member function for handling D-Bus method calls. If an empty
// scoped_ptr is returned, an empty (but successful) response will be sent.
typedef scoped_ptr<dbus::Response> (DBusManager::*DBusMethodCallMemberFunction)(
    dbus::MethodCall*);

// Class that manages dbus interactions in buffet.
class DBusManager {
 public:
  DBusManager();
  virtual ~DBusManager();

  void Init();
  void Finalize();

 private:
  // Connects to the D-Bus system bus and exports methods.
  void InitDBus();
  void ShutDownDBus();

  // Exports |method_name| and uses |member| to handle calls.
  void ExportDBusMethod(const std::string& method_name,
                        DBusMethodCallMemberFunction member);

  // Callbacks for handling D-Bus signals and method calls.
  scoped_ptr<dbus::Response> HandleTestMethod(dbus::MethodCall* method_call);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* buffet_dbus_object_;  // weak; owned by |bus_|

  DISALLOW_COPY_AND_ASSIGN(DBusManager);
};

}  // namespace buffet

#endif  // BUFFET_DBUS_MANAGER_H_
